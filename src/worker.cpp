/*
   worker.cpp 

 */

#include "worker.h"
#include <arpa/inet.h>
#include <cassert>

/*initialize worker connections */
void worker_connection_initialize(worker*p_worker,uint32_t max_connection_sizes)
{

	assert(p_worker!=NULL);
	p_worker->conn_max_size=max_connection_sizes;
	p_worker->cur_connection_number=0;
	p_worker->conn= (connection **) malloc (p_worker->conn_max_size * sizeof(*p_worker->conn));
	memset(p_worker->conn,0, p_worker->conn_max_size*sizeof(*p_worker->conn));
	return ;	
}


/* event handler when unix domain socket is readable */
int unix_domain_socket_handle(int fd, void * ctx, int events)
{
	//read connection socket file descriptor from unix domain socket
	worker * srv_worker=(worker*)ctx;
	assert(srv_worker!=NULL);
	if(events & EPOLLHUP ){
        /*   parent process minihttpd server has close the unix domain socket,
			 it usually happend at parent process has terminated .
			 we can not receive any client request any more 
		 */
		fdevents_unset_event(srv_worker->ev,fd);
		return 0;		
	}
	else if(events & EPOLLERR){
		  /*
			  error happend on unix domain socket
			  and we need to ignore this events 
		   */
		return -1;
	}
	int connection_fd;
	int n= unix_domain_socket_recvfd(fd,&connection_fd);
	if(n<=0){
		minihttpd_running_log(srv_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR,__FILE__,
							  __LINE__,__FUNCTION__,"failed to recvive file descriptor from unix domain socket!");
		return -1;
	}
	// create a new connection and register the connection_fd to epoll event loop
	connection * conn= worker_get_new_connection(srv_worker);
	if(conn==NULL){
		/*
		    we already can not handle the connection now,we just close the connection fd
			but it is better to tell client : service is unavailable now 
		  
		 */
		close(connection_fd);
		return -1;		
	}

	conn->conn_socket_fd=connection_fd;
	conn->p_worker=srv_worker;
			
	socklen_t  client_addr_length=sizeof(conn->client_addr);
	if( getpeername(connection_fd,&conn->client_addr ,&client_addr_length)==0){
		char client_addr[256]={0};
        inet_ntop(  ((struct sockaddr_in *)&conn->client_addr)->sin_family,
					&( ((struct sockaddr_in*)&conn->client_addr)->sin_addr),
				  client_addr ,sizeof(client_addr));		
		minihttpd_running_log(srv_worker->log_fd, MINIHTTPD_LOG_LEVEL_INFO, __FILE__,__LINE__,
							  __FUNCTION__,"accept a new http connection request from :%s",client_addr);      
		
	}
	//register connection readable event and start status machine
	fdevents_register_fd(srv_worker->ev, connection_fd,connection_event_handle,conn);
	connection_state_machine(conn);
	return 0;		
}


/*get a unsed connecection from worker */
connection * worker_get_new_connection(worker* srv_worker)
{

	assert(srv_worker!=NULL);
	if(srv_worker->cur_connection_number>=srv_worker->conn_max_size){
		return NULL	 ;
	}
	uint32_t connection_index=0;
	for(;connection_index< srv_worker->conn_max_size;connection_index++){
	  	  if(srv_worker->conn[connection_index]==NULL)
			  break;
	}
	//we should have a unsed connection
	assert(connection_index< srv_worker->conn_max_size);
	srv_worker->cur_connection_number++;
	srv_worker->conn[connection_index]= (connection*)malloc(sizeof(connection));
	connection_set_default(srv_worker->conn[connection_index]);
	srv_worker->conn[connection_index]->connection_index= connection_index;
	
	return srv_worker->conn[connection_index];
}

/* free all conenctions */
void worker_free_connectons(worker* p_worker)
{
	assert(p_worker!=NULL && p_worker->conn!=NULL);
	free( (void*)p_worker->conn) ;
	p_worker->conn=NULL;
}


/* timer experation handler  */
int worker_timer_expire_handler(int fd, void* ctx,int  events)
{
    worker * srv_worker=(worker*)ctx;
	assert(srv_worker!=NULL);

	//update current timestamp
	time_t localtime;
	time(&localtime);
	if(localtime==srv_worker->cur_ts)   return 0;
	srv_worker->cur_ts=localtime;

	//now we have a new second, we need to check if some connecitons's idle time has reached max idle time
	for(uint32_t connection_index=0; connection_index< srv_worker->conn_max_size;connection_index++) {
		connection * conn= srv_worker->conn[connection_index];
		if(!conn)  continue;

		//if connection state is CON_STATE_READ or CON_STATE_WRITE,
		//we need to check if connection has expire the max idle time
		uint32_t max_idle_time_reached=0;
		if(conn->state==CON_STATE_READ &&  conn->active_read_ts!=0 &&
		   (srv_worker->cur_ts- conn->active_read_ts)> srv_worker->global_config->max_read_idle_ts){
            connection_set_state(conn,CON_STATE_ERROR);
			max_idle_time_reached=1;
		}
		else if(conn->state==CON_STATE_WRITE &&  conn->active_write_ts!=0 && 
				(srv_worker->cur_ts-conn->active_write_ts)> srv_worker->global_config->max_write_idle_ts){
			connection_set_state(conn,CON_STATE_ERROR);
			max_idle_time_reached=1;			
		}
		else if(conn->state==CON_STATE_CLOSE &&(srv_worker->cur_ts - conn->close_timeout_ts)>
				                                MINIHTTPD_CLOSE_TIMEOUT ){
			max_idle_time_reached=1;
		}
		if(max_idle_time_reached) {
            connection_state_machine(conn);			
		}	   		
	}
	
}
