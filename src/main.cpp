/*
   main loop of minihttpd

 */

#include "server.h"
#include "daemonize.h"
#include "tcp_socket.h"
#include "unix_socket.h"
#include <sys/resource.h>
#include <errno.h>
#include <cassert>



void print_help()
{
	buffer * b=buffer_init();
	buffer_copy_string(b,
    "minihttpd is a simple,high-performance webserver\n" \
    "usage:\n" \
	"-f configuration_file     specify the configuration file you want to load for minihttpd\n" \
	"-D  specify the minihttpd do not run as a deamon process\n"  \
	"-v  print the minihttpd version information\n"  \
    "-h   show help\n");

	fprintf(stdout,(const char*)b->ptr);
	buffer_free(b);
}


int main(int argc,char **argv)
{

	server *srv=NULL;
	
	//step 1 : initialize server
	if( (srv= server_init()) ==NULL){
		fprintf(stderr,"failed to initialize server in [%s|%d|%s]\n",__FILE__,__LINE__,__FUNCTION__);
		return -1;		
	}	
	//step 2 : parameter parse
	char opt_chr;
	while(  (opt_chr=getopt(argc,argv,"f:hvD"))!=-1 ){
		switch(opt_chr){
			/*configuration file path */
		    case 'f':{
				        buffer_copy_string(srv->config->minihttp_config_filepath,optarg);
				        break;
			}
		   /* show help */
		   case 'h':{
			            print_help();
						server_free(srv);
						return 0;
		   }
		  case 'v':{
			       fprintf(stdout,"%s-%s",srv->config->service_name->ptr,srv->config->version_info->ptr);
				   server_free(srv);
			       return 0;
		   }
		  case 'D':{
			         srv->dont_daemonize=1;
			         break;			
		  }
		  default:{
			        print_help();
			        server_free(srv);
					return -1;
		  }				
		}		
 	}

    //step 3 :check if all configuraiton is legal
	if(buffer_is_empty(srv->config->service_root_dir)){
		fprintf(stderr,"[%s|%d|%s]:please specify minihttp root dir in configuration file\n",
				 __FILE__,__LINE__,__FUNCTION__);
		server_free(srv);
		return -1;		
	}
	if(srv->config->max_worker_number<=0)
		srv->config->max_worker_number=1; //we at least have one worker process

	//step4 :server started
    srv->uid=getuid();
	srv->gid=getgid();
	if(srv->uid==0){  //we are root 
		struct rlimit res_limit;
		if(getrlimit(RLIMIT_NOFILE,&res_limit)!=0){
            fprintf(stderr,"[%s|%d|%s]: failed to get file descriptor max number for current process!\n",
					__FILE__,__LINE__,__FUNCTION__);
			server_free(srv);
			return -1;			
		}

		res_limit.rlim_cur=srv->config->max_fd;
		res_limit.rlim_max=srv->config->max_fd;
		
		if(setrlimit(RLIMIT_NOFILE,&res_limit)!=0){
			fprintf(stderr,"[%s|%d|%s]: failed call setrlimit(RLIMIT_NOFILE,) for current process!\n");
			server_free(srv);
			return -1;			
		}     		
	}else{
		struct rlimit res_limit;
		if(getrlimit(RLIMIT_NOFILE,&res_limit)!=0){

			fprintf(stderr,"[%s|%d|%s]: failed to get file descriptor max number for current process!\n",
					__FILE__,__LINE__,__FUNCTION__);
			server_free(srv);
			return -1;				
		}

		if(srv->config->max_fd< res_limit.rlim_cur)
			res_limit.rlim_cur=srv->config->max_fd;
		else if(srv->config->max_fd<=res_limit.rlim_max)
			res_limit.rlim_cur=srv->config->max_fd;

		if(setrlimit(RLIMIT_NOFILE,&res_limit)!=0){
            fprintf(stderr,"[%s|%d|%s]: failed call setrlimit(RLIMIT_NOFILE,) for current process!\n");
			server_free(srv);
			return -1;			 
		}
	}

	//step 5:  become a daemon process if dont_daemonize=0;
	if(!srv->dont_daemonize){
        daemonize((const char*)srv->config->service_name->ptr); 		 
	}
	//step 6: open log file for error log, by default we use syslog.
	// if the minihttpd log filepath is specified manually or server dont_daemonize=1,
	// we set mode = LOG_MODE_FILE;
	
	if(!buffer_is_empty(srv->config->log_filename) || srv->dont_daemonize ){
		if(buffer_is_empty(srv->config->log_filename))
			buffer_copy_string(srv->config->log_filename,MINIHTTPD_DEFAULT_LOG_FILEPATH);
		
		srv->log_fd= open((const char*)srv->config->log_filename->ptr,O_WRONLY|O_CREAT|O_TRUNC,
						  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if(srv->log_fd<0){
			server_free(srv);
			return -1;
		}
		fd_close_on_exec(srv->log_fd);
        srv->mode=server::LOG_MODE_FILE;			
	}
	log_to_backend(srv,MINIHTTPD_LOG_LEVEL_INFO,"%s is start now...",(const char*)srv->config->service_name->ptr);
	
	//step 7 : create listening tcp socket(we only support ipv4 now)
    struct sockaddr_in * addr= (struct sockaddr_in*)&srv->server_addr;
	memset(addr,0,sizeof(*addr));
	addr->sin_family=AF_INET;
	addr->sin_addr.s_addr=htonl(INADDR_ANY);
	addr->sin_port=htons(srv->config->listenint_port);
	srv->listening_socket_fd= create_tcp_socket((struct sockaddr*)addr,srv->config->max_listening_number);
	if(srv->listening_socket_fd<0){
        log_to_backend(srv, MINIHTTPD_LOG_LEVEL_ERROR,"failed to create listening tcp socket on port:%d",
							 srv->config->listenint_port);
		server_free(srv);
		return -1;
	}

	/*
	     step 8: fork worker child process  and transfter accept socket file descriptor to worker process
		 the only tasks for main processis :
		        1) to call accept to wait for connection and pick one worker process to handle the connection  
				2) wait all worker  process to finish before exit.
   */

	for(uint32_t worker_process_id=0;worker_process_id< srv->worker_number;worker_process_id++){
		worker *  p_worker= &srv->p_worker[worker_process_id] ;
		//create unix domain socket 
        int unix_domain_socket_fd[2];
		if(socketpair(AF_UNIX,SOCK_STREAM,0,unix_domain_socket_fd)<0){
			log_to_backend(srv,MINIHTTPD_LOG_LEVEL_ERROR,"failed to create unix domain socket for worker%d",
						  worker_process_id);
			close(srv->listening_socket_fd);
			server_free(srv);
			return -1;			
		}
		srv->unix_domain_socket_fd[worker_process_id]= unix_domain_socket_fd[0];
        p_worker->unix_domain_socekt_fd=unix_domain_socket_fd[1];

		srv->sent_connection_number[worker_process_id]=0; 
		srv->worker_pid[worker_process_id]=  fork();
		
		if(srv->worker_pid[worker_process_id]<0){   //we can not fork worker process, this should not be happened
			close(srv->listening_socket_fd);
			server_free(srv) ;
			return -1;
		}
		else if(srv->worker_pid[worker_process_id]==0){   /*  worker process */ 
            /*we should use p_worker only in the child worker process */
			uint32_t worker_id=worker_process_id;
			worker * server_worker = &srv->p_worker[worker_id];

			/*step1 : get current file descriptor max number (it should be same as parent process
			                               which we have set the resouces)*/
		    struct rlimit limit;
			if(getrlimit(RLIMIT_NOFILE,&limit)<0){
				exit(-1);  // terminated the worker   				
			}			
			//step 2: set event handler
			server_worker->ev= fdevent_initialize(limit.rlim_cur);
            /*support max connection number */
			server_worker->conn_max_size= limit.rlim_cur/2 ;
			server_worker->cur_connection_number=0;
            server_worker->conn= (connection**) malloc(server_worker->conn_max_size * sizeof(*server_worker->conn));

			//step 3 : register unix domain socket event
			fdevents_register_fd(server_worker->ev,server_worker->unix_domain_socekt_fd,
								 unix_domain_socket_handle,server_worker);
			//EPOLLHUP |EPOLLERR events is set by default
			fdevents_set_events(server_worker->ev,server_worker->unix_domain_socekt_fd,EPOLLIN); 

			//step 4 : open log file for worker to log debug/info/warning/error
            if(buffer_is_empty(server_worker->log_filepath)){
				char  worker_log_filepath[255];
				snprintf(worker_log_filepath,sizeof(worker_log_filepath),MINIHTTPD_WORKER_CONFIG_PATH"%4u.log", worker_id);
				buffer_append_string(server_worker->log_filepath,worker_log_filepath);			   				
			}
			server_worker->log_fd= open((const char*)server_worker->log_filepath->ptr, O_WRONLY|O_CREAT|O_TRUNC,
				 S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
			if(server_worker->log_fd<0){
				exit(-2); 
			}
			uint32_t worker_main_loop=1;
			/* main loop for worker: epoll event loop for unix domain socket and connections */
			while(worker_main_loop){
				int n=epoll_wait(server_worker->ev->epoll_fd, server_worker->ev->epoll_events,
								 server_worker->ev->max_epoll_events,-1);
				if(n<0 ){
					if(errno!=EINTR){
						minihttpd_running_log(server_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR
											  ,__FILE__,__LINE__,__FUNCTION__,
											  "failed to call epoll with errno=%d",errno);						
					}
					continue;
				}
				else if(n==0){
					//we should not get to here
					continue;					
				}else{

					for(uint32_t event_index=0;event_index<n;event_index++){
                        struct epoll_event * event= &server_worker->ev->epoll_events[event_index];
						assert(event!=NULL);
						int connection_socket_fd= event->data.fd;
						event_handle handler=fdevents_get_events(server_worker->ev,connection_socket_fd);
						void * event_ctx=fdevents_get_context(server_worker->ev, connection_socket_fd);
						assert(handler!=NULL);
						int handle_status= handler(connection_socket_fd,event_ctx,event->events);
						
						
					}										
				} 
			}

			
  			
	
		}
        //parent process
		log_to_backend(srv,MINIHTTPD_LOG_LEVEL_INFO,"worker process %d is already created!\n",worker_process_id);		
	}

	uint32_t main_loop=1;
    //main loop to accept client connection and re-transfter to worker process 
	while(main_loop) {
         /*
            signal handler here 

		 */
		
		//we block here to wait connection(only IPV4 is supported now ) 
		struct sockaddr_in client_addr;
		socklen_t client_addr_length=sizeof(client_addr);
		int connection_fd =accept(srv->listening_socket_fd,(struct sockaddr*)&client_addr,(socklen_t*)& client_addr_length);
		if(connection_fd<0){
			switch(errno){
			   case  EINTR:
				// the connection is reset by client
			   case ECONNABORTED:   
				                 continue;				   		  				
			   case  EMFILE:  //file descriptor is all used now, need to send file descriptor to worker soon
				             break;
			   default:  {
				   log_to_backend(srv,MINIHTTPD_LOG_LEVEL_ERROR,"failed to call accept() with errno=%d\n",errno);
				   main_loop=0;
				   break;				
			   }				
			}			
		}
		else{
		  /*
		     pick up a worker process and send the @conneciton_fd to it
             the pick algorithm is round-robin,;
			 but for the draft version, we just pick a worker that we has sent the min connections  

		  */
		 uint32_t  pick_worker_index=0;
		 uint32_t  min_sent_connections=srv->sent_connection_number[pick_worker_index];
         for(uint32_t worker_process_id=1; worker_process_id<srv->worker_number;worker_process_id++){
            if(srv->sent_connection_number[worker_process_id] < min_sent_connections){
				min_sent_connections= srv->sent_connection_number[worker_process_id];
				pick_worker_index= worker_process_id;
			}			
		 }

		 /*set file descriptor to nonblocking and set close_on_exec flag*/
		 fd_set_nonblocking(connection_fd);
		 fd_close_on_exec(connection_fd);

		 if(unix_domain_socket_sendfd(srv->unix_domain_socket_fd[pick_worker_index],
									 connection_fd)<0){
			 log_to_backend(srv,MINIHTTPD_LOG_LEVEL_ERROR,"failed to send the connection file descriptor to worker!");
			close(connection_fd);  //just close it to tell the client,we can not handle it now.
			continue;			
		 }
		 srv->sent_connection_number[pick_worker_index]++;
		 //close the file descriptor as it is already marked in flight
		 close(connection_fd);
	   }
	}





	

	
}
