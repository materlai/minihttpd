/*
    connection.cpp : handle tcp connection events with status machine 

 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <errno.h>
#include "connection.h"
#include "worker.h"
#include "log.h"

#include <cassert>



/* update state for connection  */
void connection_set_state(connection*conn,connection_state_t state)
{
	  assert(conn!=NULL);
	  conn->state=state;
}

/*retrive connecection state */
buffer * connection_get_state(connection *conn)
{
	buffer * b= buffer_init();
	assert(conn->state>=CON_STATE_REQUEST_START && conn->state<=CON_STATE_CLOSE);
	const char * connection_support_state[]= {
			"connecection_state_request_start",
			"connecection_state_request_end",
			"connecection_state_read",
			"connecection_state_handle_request",
			"connecection_state_response_start",
			"connecection_state_write",
			"connecection_state_response_end",
			"connecection_state_error",
			"connection_state_closed"		
	};
	buffer_copy_string(b,connection_support_state[conn->state]);
	return b;
} 


/*event handle when connection has the events occurs */
int connection_event_handle(int fd, void *handler_ctx,int events)
{
	connection *conn =(connection*)handler_ctx;
	assert(conn!=NULL);
	if(events &  EPOLLIN)
		conn->readable=1;
	if(events & EPOLLOUT)
		conn->writeable=1;
	//it seem some error has happend
	if(events & ~(EPOLLIN|EPOLLOUT) ){
        if(events& EPOLLHUP) {   //peer socket is closed 
			if(conn->state==CON_STATE_CLOSE) {
				/* we can release all resouce for the connection socket file descriptor */
				close(conn->conn_socket_fd);
				/*we can free all conneciton resource now ( file descriptor, conneciton,fdevent handle)*/
				
			}
			else{   /* error occurs */
				connection_set_state(conn,CON_STATE_ERROR);
								
			}			
		}else if(events &EPOLLERR){
			/*some errors has happenned */
 		   connection_set_state(conn,CON_STATE_ERROR);
		   
		}else {
			   /* we should not come to here */	   	
		}		
	}

   /*call connection_state_machine() to handle the actually socket IO */
    return connection_state_machine(conn);
}


/* connection socket event handle state machine  */
int connection_state_machine(connection * conn)
{
    assert(conn!=NULL && conn->p_worker!=NULL);
	worker * srv_worker= conn->p_worker;

	/*
         start to handle connection socket according state    
	 */
	uint32_t handle_done=0;
	while(!handle_done) {
		
		connection_state_t old_state= conn->state;
	  	buffer * cur_state=connection_get_state(conn);
	    minihttpd_running_log(srv_worker->log_fd,MINIHTTPD_LOG_LEVEL_INFO,__FILE__,__LINE__,
						  __FUNCTION__, "connection fd=%d has state:%s",conn->conn_socket_fd,
						  (const char*)cur_state->ptr);
	    buffer_free(cur_state);
	        
		switch(conn->state) {
			/*  request start state:the first state for the connection
				we record the timestamp to jump to CON_STATE_READ to read client request
			*/
		   case CON_STATE_REQUEST_START: {
			   time(&conn->start_ts);
			   connection_set_state(conn,CON_STATE_READ);
			   break;
		   }
		   /*  client request has been read,try to parse the request,
			   if client is successfully parsed,jump to CON_STATE_HANDLE_REQUEST state)
		   */
		   case  CON_STATE_REQUEST_END:{
                			   /*reset request  all field if we are in keep-alive mode */
			   buffer_reset(conn->connection_request.http_method);
			   conn->connection_request.http_version=HTTP_VERSION_UNSET;
			   buffer_reset(conn->connection_request.http_range);
			   buffer_reset(conn->connection_request.hostname);
			   buffer_reset(conn->connection_request.request_url);

			   if(http_reuqest_parse(conn)==0){
                   /* log the http request head parse result when parse is successfully done  */			   
				   log_parsed_request(conn, &conn->connection_request);				   
			   }			   
			   connection_set_state(conn,CON_STATE_HANDLE_REQUEST);				   
			   break;
		   }
		   /*
			   handle the actually connection request in this state ,
			   after requestd is handled, jump to response start state and 
			   send back the handle result
		   */
		   case CON_STATE_HANDLE_REQUEST :{

			   break;
		   }
		   /* connection socket request haa been handled and send back the handle request  */
		   case CON_STATE_RESPONSE_START:{

			
		   }
		   /*
                conenction  response has all sent back to the client 
		   */   
		   case CON_STATE_RESPONSE_END:{


			
		   }
		   /*
               read  client request from conenction socket kernel buffer
			   and check if we have read the completed request.
			   if yes,jump to request end state 
			*/
		   case CON_STATE_READ:{
			   connection_handle_read_state(conn);
			   break;			
		   }
		   /*
			   send the response message to the client(http head,request handle result...).
			   after all response message is sent back, jump to response end state.
			   
			*/
		   case CON_STATE_WRITE:{
                
			
		   }
		   /*
              some error has occues during connection socket IO or request handler
			*/
		   case CON_STATE_ERROR:{

			
		   }
		   /* the last state for the connection socket,
			  if keep_alive is false, we close the connecection socket
			  else reset the connection and wait for client the next request 
			*/
	  	   case CON_STATE_CLOSE:{


			
		   }
		   default:{
			     /*should not come to here */
			     break;
		   }		   			
		}
		
		handle_done = conn->state== old_state?1:0 ;		  
	}

	/*do something */
    
}



/*
     handle  connection socket read
     socket IO error may happend while reading
	 return value:   >0 for sucess
	                 0 mean remote socket has been closed 
	                -1 mean error happen but we can reuse the socket (interrupt by signal or socket buffer is not ready)
					-2 mean error happens and we can not use the socket again ; 
 */
int connection_handle_read(connection * conn)
{

	uint32_t  need_read_bytes=0;
	if(ioctl(conn->conn_socket_fd,FIONREAD,&need_read_bytes)!=0 ||  need_read_bytes==0 ||
	   need_read_bytes<1024){
	        need_read_bytes=1024;
	}
    
	uint8_t * ptr=NULL;
	chunk_get_memory(conn->readqueue,&ptr,&need_read_bytes);
	assert(ptr!=NULL);
	int n=read(conn->conn_socket_fd, ptr, need_read_bytes);
	conn->readable=0;
	if(n<0){
		if(errno==EINTR || errno ==EAGAIN){
            if(errno==EINTR)   conn->readable=1;
			return -1;
		}
		return -2;		
	}else if(n==0){
		/*the remote socket has been closed */
	    return 0;			
	}else {
        /* we have successfully read socket kernel buffer  */
#if 0	
		buffer * b= buffer_init();
		buffer_copy_string_length(b,(const char*)ptr,n);
		minihttpd_running_log(conn->p_worker->log_fd, MINIHTTPD_LOG_LEVEL_INFO,__FILE__,__LINE__,
							  __FUNCTION__,"read socket buffer:%s",(const char*)b->ptr);
		buffer_free(b);
#endif
		chunk_commit_memory(conn->readqueue,n);
		if(n==need_read_bytes)
			conn->readable=1;		
	}
	return n;
}

/* handle conneciton socket read request   */
int connection_handle_read_state(connection * conn)
{
	 assert(conn!=NULL);
	 if(!conn->readable)  return -1;
	 int n= connection_handle_read(conn);
	 if(n==-2){
			 connection_set_state(conn,CON_STATE_ERROR);
			 return n;
	 }
	 else if(n==-1){
		 return -1;  //we do not read any new socket buffer		 
	 }
	 else if(n==0){
             connection_set_state(conn,CON_STATE_CLOSE);
			 return n;				 
	 }	 
	 // start to check if we have receive \r\n\r\n
	 chunk * c= conn->readqueue->first;
	 chunk * last_chunk=NULL;
	 uint32_t last_chunk_index=0;
	 for(;c!=NULL ; c=c->next){
		 assert(c->chunk_type==CHUNK_MEM) ;  //readqueue all chunk is chunk_mem type
		uint32_t chunk_string_length= buffer_string_length(c->mem);
		for(uint32_t index=0;index<chunk_string_length;index++) {
            char buf_c= c->mem->ptr[index];
			if(buf_c=='\r') { //it maybe can be the begining of \r\n\r\n;
				uint32_t j=index+1;
				chunk * cc=c;
				const char target_string[]="\r\n\r\n";
				uint32_t match_index=1;
				for(;cc!=NULL ; cc=cc->next,j=0) {
					uint32_t remaining_len= buffer_string_length(cc->mem);
					for(; j<remaining_len;j++){
						if(cc->mem->ptr[j]==target_string[match_index]){
							match_index++;
							if(match_index>=sizeof(target_string)-1){
								last_chunk=cc;
								last_chunk_index=j+1;
								goto found;
							}
						}else{
							goto retry;							
						}		
					}					
				}				
				
			}
retry:
		 ;	
		}		 
	 }

found:	 
	 if(last_chunk){  //we have read a completed client request
		 buffer_reset(conn->connection_request.request_content);
	     chunk * c= conn->readqueue->first;
		 for(;c!=NULL ; c=c->next){
             uint32_t copy_len= buffer_string_length(c->mem);
			 if(c==last_chunk){
				 copy_len= last_chunk_index;				 
			 }
			 buffer_append_string_length(conn->connection_request.request_content,
								  (const char*)c->mem->ptr, copy_len);
			 if(c==last_chunk)  break;			 
		 }
#if 0
		 minihttpd_running_log(conn->p_worker->log_fd, MINIHTTPD_LOG_LEVEL_INFO,
							   __FILE__,__LINE__,__FUNCTION__, "http request head is read completed:%s",
							   (const char*)conn->connection_request.request_content->ptr); 
#endif 
		 connection_set_state(conn,CON_STATE_REQUEST_END);		 
	 }else {
		  /*
		    check if the readqueue buffer size is larger than  max http request head
		     if yes, but we still do not get the http head.
			 then send http status=414;
		  */
		 if(chunkqueue_length(conn->readqueue)>conn->p_worker->global_config->max_http_head_size){
			 minihttpd_running_log(conn->p_worker->log_fd, MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__
								   ,"too larger http request head to handle!");
			 conn->http_status=414;
			 conn->keep_alive=0;
			 connection_set_state(conn,CON_STATE_HANDLE_REQUEST);
		 }
	 }
	 return n; 	
}

/*  handle connection write state */
int connection_handle_write_state(connection * conn)
{

	

	
}

