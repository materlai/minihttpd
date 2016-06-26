/*
    connection.cpp : handle tcp connection events with status machine 

 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <time.h>
#include <errno.h>
#include "connection.h"
#include "worker.h"
#include "log.h"



#include <cassert>

/* set connection all filed to default  */
void connection_set_default(connection * conn)
{

	assert(conn!=NULL);
	conn->state=CON_STATE_REQUEST_START;
	conn->conn_socket_fd=-1;
    conn->readable=conn->writeable=0;
	conn->keep_alive=0;
	conn->http_status=0;
	conn->http_response_body_finished=0;

	conn->readqueue= chunkqueue_init();
	conn->writequeue=chunkqueue_init();
	request_initialize(&conn->connection_request);
	response_initialize(&conn->connection_response);

	conn->p_worker= NULL;
	
}

/* reset connection to default state */
void connection_reset(connection* conn)
{
	assert(conn!=NULL);
	
	conn->readable=conn->writeable=0;
	conn->http_status=0;
	conn->http_response_body_finished=0;
	conn->keep_alive=0;

	chunkqueue_reset(conn->readqueue);
	chunkqueue_reset(conn->writequeue);
	request_reset(&conn->connection_request);
	response_reset(&conn->connection_response);	
}


/* close the connection */
void connection_close(connection * conn)
{
	 assert(conn!=NULL);
     assert(conn->conn_socket_fd>=0 && conn->conn_socket_fd<conn->p_worker->ev->max_fd );
	 fdevents_unset_event(conn->p_worker->ev,conn->conn_socket_fd);
	 fdevents_unregister_fd(conn->p_worker->ev,conn->conn_socket_fd);

	 //close the connection socket
	 close(conn->conn_socket_fd);
	 conn->conn_socket_fd=-1;

	 connection_set_state(conn,CON_STATE_CONNECT);
	 
}

/*  free connection resource */
void connection_free(connection * conn)
{
	 assert(conn!=NULL);
	 chunkqueue_free(conn->readqueue);
	 chunkqueue_free(conn->writequeue);
	 
	 response_free(&conn->connection_response);
	 request_free(&conn->connection_request);
	 free( (void*) conn);
}

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
	assert(conn->state>=CON_STATE_REQUEST_START && conn->state<=CON_STATE_CONNECT);
	const char * connection_support_state[]= {
			"connecection_state_request_start",
			"connecection_state_request_end",
			"connecection_state_read",
			"connecection_state_handle_request",
			"connecection_state_response_start",
			"connecection_state_write",
			"connecection_state_response_end",
			"connecection_state_error",
			"connection_state_closed",
			"conenction_state_connect",
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
			   conn->http_status=0; /*unset http status */

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
			   response_initialize(&conn->connection_response);
			   http_request_handle(conn);
#if 1
			   if(conn->http_status==200){
			      minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_INFO,
									 __FILE__,__LINE__,__FUNCTION__,"response head is ready:\n"
										"request fullpath=%s\nrequest file length=%d\nrequest file mime-type=%s\n",
										(const char*)conn->connection_response.fullpath->ptr,
										conn->connection_response.content_length,
										(conn->connection_response.content_type->ptr!=NULL) ?
														(const char*)conn->connection_response.content_type->ptr:"unset" );
			   }else{

				   minihttpd_running_log(conn->p_worker->log_fd, MINIHTTPD_LOG_LEVEL_ERROR,
									 __FILE__,__LINE__,__FUNCTION__, "invalid http request with response http status=%d\n",
										 conn->http_status);				    
			   }
#endif
			   connection_set_state(conn,CON_STATE_RESPONSE_START);
			   break;
		   }
			   
		   /* connection socket request has been handled and send back the handle request  */
		   case CON_STATE_RESPONSE_START:{
			   /* start to write http response head to client according to conn->http_status  */
			   connection_handle_prepare_response_head(conn);
			   connection_set_state(conn,CON_STATE_WRITE);
			   break;
		   }
		   /*
                conenction response has all sent back to the client
				1) if keep-alive flags is set, reset the connection to request start state
				2) if not set,call shutdown() to send FIN and wait for the timeout to close the socket 
		   */   
		   case CON_STATE_RESPONSE_END:{
			   /* all data in writequeue is already sent out */
			   if(conn->keep_alive){   /* we need to wait for next http request */
                   connection_set_state(conn,CON_STATE_REQUEST_START);
	   			   connection_reset(conn);
				   
			   }else{

				    /* we neend to close the socket after specified time */
				    if(shutdown(conn->conn_socket_fd,SHUT_WR)==0){
						/*
						     set the timeout for close							 
						*/
						time(&conn->close_timeout_ts);
						connection_set_state(conn,CON_STATE_CLOSE);
				    }else{
						   time_t  local_time;
						   time(&local_time);
						   conn->close_timeout_ts= local_time-(MINIHTTPD_CLOSE_TIMEOUT+1);  //we need close the socket soon 
						   connection_set_state(conn,CON_STATE_CLOSE);    					
					}
					connection_reset(conn);
			   }
			   break;			
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
			   if(chunkqueue_length(conn->writequeue)!=0){
				   if(conn->writeable){
					   connection_handle_write_state(conn);
				   }				   
			   }else if(conn->http_response_body_finished==1){
                   /* we have send all writequeue data */
				   connection_set_state(conn,CON_STATE_RESPONSE_END) ;			   				   
			   }
			   
			   break;
		   }
		   /*
              some error has occues during connection socket IO or request handler
			*/
		   case CON_STATE_ERROR:{

			
		   }
		   /* the last state for the connection socket,
			   1) read the socket kernel buffer and drop them
			   2) if close_timeout is expried,call close() to close the socket file descriptor
			*/
	  	   case CON_STATE_CLOSE: {
			   //try to read socket buffer which is remaining in kernel buffer

			   time_t localtime;
			   time(&localtime);					   
			   {
                   uint8_t  remaining_buf[512];
				   int n=read(conn->conn_socket_fd,remaining_buf,sizeof(remaining_buf));
				   if(n==0 || (n<0 && errno!=EINTR && errno!=EAGAIN)){		 
					   conn->close_timeout_ts= localtime-(MINIHTTPD_CLOSE_TIMEOUT+1);							  
					   
				   }					    				   
			   }
               
			   if(localtime-conn->close_timeout_ts>MINIHTTPD_CLOSE_TIMEOUT){
                   connection_close(conn);				   				   
			   }
			   break;			
		   }
		    /*
			   the whole http request and http handle is finished and local socket file descriptor is already closed,
			   after jump to CON_STATE_CONNECT means we need to free the connection resource soon  
			 */	   
		   case CON_STATE_CONNECT:{
			   /*  */
			   break; 			
		   }   
		}
		
		handle_done = conn->state== old_state?1:0 ;		  
	}

    //if the connectionn is already closed, then free connection 
	if(conn->state==CON_STATE_CONNECT){
		
		uint32_t connection_index= conn->connection_index;
		conn->p_worker->conn[connection_index]=NULL;
		conn->p_worker->cur_connection_number--;
		connection_free(conn);
		return 0;
	}

    /* the loop for state is finished for this events */
	switch(conn->state){
	  case CON_STATE_READ:
	  case CON_STATE_CLOSE:{
		  /* set epoll events to EPOLLIN */
		  fdevents_set_events(conn->p_worker->ev,conn->conn_socket_fd,EPOLLIN);
		  break;		
	  }
	  case CON_STATE_WRITE:{
		  if(chunkqueue_length(conn->writequeue)!=0 && conn->writeable==0){
			  fdevents_set_events(conn->p_worker->ev, conn->conn_socket_fd,EPOLLOUT);			  
		  }else{
			  fdevents_unset_event(conn->p_worker->ev,conn->conn_socket_fd);			  
		  }
		  break;		  
	  }
 	  default:{
           /*   we should not be here    */
		  break;
	  }
	}	

	return 0;
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


/*
     prepare http response head  when http_status!=200

 */
int connection_handle_prepare_response_head(connection * conn)
{
	  assert(conn!=NULL);
	  if(conn->http_status==0)  /*  it  should can not be happend */
		  conn->http_status=403;

	  /* custom respons body message according to http_status */
	  switch(conn->http_status){
	   case 204:
	   case 205:
	   case 304:{   /* do not need custom body message for these error status */
		       chunkqueue_reset(conn->writequeue);
			   conn->http_response_body_finished=1;
			   break;
	   }
	   default:{
             /* custom response content body when  http_status is between 400 ~ 600  */
		   if(conn->http_status<400 || conn->http_status>=600)  break;

		   buffer * b =buffer_init();
		   const char * http_error_content="<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"
					   "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n"
					   "         \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
					   "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
					   " <head>\n"
			           "  <title>";
		   buffer_append_string(b, http_error_content);
		   char http_status[32];
		   snprintf(http_status,sizeof(http_status), "%d", conn->http_status );
		   buffer_append_string(b,http_status);
		   buffer_append_string(b," - ");
		   key_map * http_status_map=query_http_status_string(conn->http_status);
		   assert(http_status_map!=NULL);
		   buffer_append_string(b, http_status_map->map_string);

           buffer_append_string(b,
						 "</title>\n"
					     " </head>\n"
					     " <body>\n"
						 "  <h1>");
		   buffer_append_string(b,http_status);
		   buffer_append_string(b," - ");
		   buffer_append_string(b, http_status_map->map_string);

		   buffer_append_string(b, "</h1>\n"
					               " </body>\n"
								   "</html>\n");
		   chunkqueue_append_buffer(conn->writequeue, b );
		   buffer_free(b); 		   		   
		   conn->http_response_body_finished=1;
		   buffer_copy_string(conn->connection_response.content_type,"text/html");
		   break;
	    }
	 }

	 /*  set content-length to  writequeue size  */
	if(conn->http_response_body_finished==1){
		uint32_t queue_len= chunkqueue_length(conn->writequeue);
        conn->connection_response.content_length= queue_len;
	}
	return connection_write_response_head(conn);
}



/*send http response status and http response head to client  */
int connection_write_response_head(connection* conn)
{
	assert(conn!=NULL);
	buffer * b =buffer_init();
	/* prepare http response status line  */
	if(conn->connection_request.http_version==HTTP_VERSION_1_1 )
		buffer_append_string(b,"HTTP/1.1 ");
	else buffer_append_string(b,"HTTP/1.0 ");
	char http_status[32];
	snprintf(http_status,sizeof(http_status),"%d ",conn->http_status);
	buffer_append_string(b,http_status);

	key_map * http_status_map= query_http_status_string(conn->http_status);
	assert(http_status_map);
	buffer_append_string(b,http_status_map->map_string);

	/* prepare http response head :
       1) Connection: keep-alive/close
	   2) content-length: if content-length >0
	   3) Transfter-Encoding  -> chunked  (not supported yet)
	   4) content-type : mime type string 
	*/
    if(conn->connection_request.http_version!=HTTP_VERSION_1_1 || conn->keep_alive==0){
			buffer_append_string(b,"\r\nConnection: ");
            if(conn->keep_alive)  buffer_append_string(b,"keep-alive");
			else buffer_append_string(b,"close");		
	}

	if(conn->connection_response.content_length>0){
		buffer_append_string(b,"\r\nContent-Length: ");
		char content_length[32];
		snprintf(content_length,sizeof(content_length),"%d",conn->connection_response.content_length);
		buffer_append_string(b,content_length); 
	}
    if(!buffer_is_empty(conn->connection_response.content_type)){
		buffer_append_string(b,"\r\nContent-Type: ");
		buffer_append_string(b, (const char*)conn->connection_response.content_type->ptr);
	}else{
		
           buffer_append_string(b,"\r\nContent-Type: application/octet-stream");
	}

	/* append DATE & server name to http response head */
	buffer_append_string(b,"\r\nDate: ");
	char current_timestamps[128];
	time_t ts;
	time(&ts);
	int n=strftime(current_timestamps,sizeof(current_timestamps),"%Y-%m-%d %H:%M:%S",localtime(&ts));
	assert(n<sizeof(current_timestamps));
	buffer_append_string(b,current_timestamps);

	buffer_append_string(b,"\r\nServer: ");
	buffer_append_string(b,(const char*)conn->p_worker->global_config->service_name->ptr);
	buffer_append_string(b,"\r\n\r\n");

#if 1
    minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_INFO,
						  __FILE__,__LINE__,__FUNCTION__, "http response head:%s", (const char*)b->ptr);
	
#endif 	
	
    /*  now the http request head is ready now  */
    chunkqueue_prepend_buffer(conn->writequeue,b);
	buffer_free(b);
	return 0;	
}


/*
   call writev to write buffer to socket 

 */

int connection_writev_mem_chunk(connection * conn, chunkqueue * queue)
{
    assert(conn!=NULL && queue !=NULL);
	assert(queue->first!=NULL && queue->first->chunk_type==CHUNK_MEM);

	const int MAX_CHUNKS=16;
	struct iovec  send_chunks[MAX_CHUNKS];

	chunk * c =queue->first;
    uint32_t chunk_index=0;
	uint32_t send_bytes=0;
	for( ; c!=NULL &&c->chunk_type==CHUNK_MEM && chunk_index<MAX_CHUNKS ; c=c->next){
		assert(c->chunk_offset>=0 && c->chunk_offset<= buffer_string_length(c->mem));
		uint32_t chunk_len= buffer_string_length(c->mem)-c->chunk_offset;
		if(chunk_len> 0){
			send_chunks[chunk_index].iov_base= c->mem->ptr +  c->chunk_offset;
			send_chunks[chunk_index].iov_len= chunk_len;
			send_bytes+=chunk_len;
			chunk_index++;
		}
	}

	if(chunk_index==0){
		chunkqueue_remove_finished_chunk(queue);
		return 0;		
	}

	int n= writev(conn->conn_socket_fd,send_chunks,chunk_index);
	if(n<0){
		switch(n){
		case EINTR:
		case EAGAIN:  return -1;
		case EPIPE:
		case ECONNRESET: 
		default:
			    return -2;			
		}		
	}
	
	if(n>0){
		chunkqueue_mark_written(queue,n);		
	}

	return  n==send_bytes? 0: 1  ;	
}


/* call sendfile to socket buffer   */

int connection_send_file_chunk(connection * conn,chunkqueue * queue)
{

	assert(conn!=NULL && queue!=NULL);
	assert(queue->first!=NULL && queue->first->chunk_type==CHUNK_FILE);
    assert(  queue->first->chunk_offset  <= queue->first->send_file.length);

	uint32_t offset=   queue->first->send_file.offset+ queue->first->chunk_offset;
	uint32_t send_len= queue->first->send_file.length-queue->first->chunk_offset  ;

	if(send_len==0){
        chunkqueue_remove_finished_chunk(queue);
		return 0;		
	}

	if(queue->first->send_file.file_fd<0){
        queue->first->send_file.file_fd= open((const char*)queue->first->send_file.filename->ptr,O_RDONLY);
		if(queue->first->send_file.file_fd<0)  return -1;
	}

	/*  call sendfile to send file content to socket */
	int r= sendfile(conn->conn_socket_fd, queue->first->send_file.file_fd,(off_t*)&offset,send_len);
	if(r<0){
        switch(errno){
		  case EINTR:
		  case EAGAIN:   return -1;
		  case EPIPE:
		  case ECONNRESET:
		  default:  return -2;
		}
	}
	
	if(r>0) {
        chunkqueue_mark_written(queue,r);				
	}

	return r==send_len? 0:1;	
}



/*
     handle connection write : send data in writequeue to remote socket
	 return value:  0:   all data is sent in writqueue
	                1:   part of data in writequeue is written
				   -1: some error happend and we can contonue to write later
				   -2: some critical error has happend,we need to close the socket 

 */

int connection_handle_write(connection* conn)
{
	assert(conn!=NULL);
	if(!conn->writeable)  return -1;

	chunk * c= conn->writequeue->first;
	for(;c!=NULL;c=conn->writequeue->first ){
        int n=-1;
		switch(c->chunk_type){
		   case CHUNK_MEM:{
			   n=connection_writev_mem_chunk(conn,conn->writequeue);
			   break;		
		   }
		   case CHUNK_FILE:{
               n= connection_send_file_chunk(conn,conn->writequeue);
			   break;	
		   } 	
		   default:{
			             assert(0);  //abort here if we do not set chunk type.
		   }	
	    }

		if(n!=0){
			return n;			
		}
	}
 
	return 0;	
}



int connection_handle_write_state(connection * conn)
{
	assert(conn!=NULL);
	if(!conn->writeable)  return -1;

	chunkqueue  * queue= conn->writequeue;

	int n= connection_handle_write(conn);
	switch(n){
	  case 0:  /* all data in writequeue is written*/
		     if(conn->http_response_body_finished==1){
			     connection_set_state(conn,	CON_STATE_RESPONSE_END);
		     }
			 break;
	  case -1:/* we need wait for EPOLLOUT events */		 
	  case 1:  /* parf of data in writequeue is written  */
		     conn->writeable=0;
		     break;
	  case -2:
	      	  connection_set_state(conn,CON_STATE_ERROR);
		      break;
	}
	return 0;
	
}

