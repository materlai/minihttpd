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
	fdevents_set_events(srv_worker->ev,connection_fd, EPOLLIN);
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
