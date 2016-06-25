/*
   main loop of minihttpd

 */

#include "server.h"
#include "daemonize.h"
#include "tcp_socket.h"
#include "unix_socket.h"
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>
#include <cassert>


static uint32_t signal_pipe_handled=0;
static uint32_t signal_child_handled=0;


void print_help()
{
	buffer * b=buffer_init();
	buffer_copy_string(b,
    "minihttpd is a simple,high-performance webserver\n" \
    "usage:\n" \
	"-f  configuration_file     specify the configuration file you want to load for minihttpd\n" \
	"-D  specify the minihttpd do not run as a deamon process\n"  \
	"-v  print the minihttpd version information\n"  \
    "-h  show help\n");

	fprintf(stdout,"%s",(const char*)b->ptr);
	buffer_free(b);
}
//signal handler
void  signal_handler(int signo)
{  
	switch(signo){
	  case SIGPIPE:
		             signal_pipe_handled=1;
		             break;
	  case SIGCHLD:  signal_child_handled=1;
		             break;
	  default:       //we should not come to here  
		             break;  
   }
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
				        buffer_copy_string(srv->config->minihttpd_global_config_filepath,optarg);
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
	if(srv->config->max_worker_number<=0){
		srv->config->max_worker_number=1; //we at least have one worker process
		srv->worker_number=srv->config->max_worker_number;
	}
	
    /*parse the mime configuration file */
	if(buffer_is_empty(srv->config->mimetype_filepath)
	   ||   (srv->config->table= mime_table_initialize( (const char*)srv->config->mimetype_filepath->ptr) )==NULL){
        fprintf(stderr,"invalid mime configuration file is specified,pls check it..\n");
		server_free(srv);
		return -1;
	}

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
			fprintf(stderr,"[%s|%d|%s]: failed call setrlimit(RLIMIT_NOFILE,) for current process!\n",
					__FILE__,__LINE__,__FUNCTION__);
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
            fprintf(stderr,"[%s|%d|%s]: failed call setrlimit(RLIMIT_NOFILE,) for current process!\n",
				__FILE__,__LINE__,__FUNCTION__);
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
	    step 8:  setup signal  handler
		signo: SIGCHLD
		signo: SIGPIPE: unix domain socket pipe is broken 
	 */
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags=0;
	act.sa_handler=signal_handler;
	sigaction(SIGPIPE,&act,NULL);
	sigaction(SIGCHLD,&act,NULL);

	/*
	     step 9: fork worker child process  and transfter accept socket file descriptor to worker process
		 the only tasks for main processis :
		        1) to call accept to wait for connection and pick one worker process to handle the connection  
				2) wait all worker  process to finish before exit.
   */
    
	for(uint32_t worker_process_id=0;worker_process_id< srv->worker_number ;worker_process_id++){
		server_child * child= &srv->child[worker_process_id];
		
		//create unix domain socket 
		if(socketpair(AF_UNIX,SOCK_STREAM,0,child->unix_domain_socket_fd)<0){
			log_to_backend(srv,MINIHTTPD_LOG_LEVEL_ERROR,"failed to create unix domain socket for worker%d",
						  worker_process_id);
			close(srv->listening_socket_fd);
			server_free(srv);
			return -1;			
		}
        child->sent_connection_number=0;
		int unix_domain_socket_child_fd=child->unix_domain_socket_fd[1]; 
		child->pid=fork();
		
		if(child->pid <0){   //we can not fork worker process, this should not be happened
			close(srv->listening_socket_fd);
			server_free(srv) ;
			return -1;
		}
		else if(child->pid ==0) {   /*  worker process */ 
            /*we should use p_worker only in the child worker process */
#if 0
			minihttpd_running_log(srv->log_fd,MINIHTTPD_LOG_LEVEL_INFO,__FILE__,__LINE__,__FUNCTION__,
								  "worker(pid=%d) is starting.....",getpid());
#endif 	
			worker * server_worker =  (worker*)malloc(sizeof(worker));
			memset(server_worker,0,sizeof(worker));
            server_worker->worker_id= worker_process_id;
			server_worker->unix_domain_socekt_fd=unix_domain_socket_child_fd;
			server_worker->log_filepath=buffer_init();
			server_worker->global_config= srv->config;
			/*step1 : get current file descriptor max number (it should be same as parent process
			                               which we have set the resouces)*/
		    struct rlimit limit;
			if(getrlimit(RLIMIT_NOFILE,&limit)<0){
				exit(-1);  // terminated the worker   				
			}			
			//step 2: set event handler
			server_worker->ev= fdevent_initialize(limit.rlim_cur);
            /*support max connection number */
			uint32_t worker_support_max_connections=limit.rlim_cur/2;
			worker_connection_initialize(server_worker, worker_support_max_connections);
			
			//step 3 : register unix domain socket event
			fdevents_register_fd(server_worker->ev,server_worker->unix_domain_socekt_fd,
								 unix_domain_socket_handle,server_worker);
			//EPOLLHUP |EPOLLERR events is set by default
			fdevents_set_events(server_worker->ev,server_worker->unix_domain_socekt_fd,EPOLLIN); 

			//step 4 : open log file for worker to log debug/info/warning/error
            if(buffer_is_empty(server_worker->log_filepath)){
				char  worker_log_filepath[255];
				snprintf(worker_log_filepath,sizeof(worker_log_filepath),
						            MINIHTTPD_WORKER_CONFIG_PATH"%u.log", server_worker->worker_id );
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
						event_handle handler=fdevents_get_handle(server_worker->ev,connection_socket_fd);
						void * event_ctx=fdevents_get_context(server_worker->ev, connection_socket_fd);
						assert(handler!=NULL);
						int handle_status= handler(connection_socket_fd,event_ctx,event->events);
					    minihttpd_running_log(server_worker->log_fd,handle_status==0?
											  MINIHTTPD_LOG_LEVEL_INFO:MINIHTTPD_LOG_LEVEL_ERROR,
											  __FILE__,__LINE__,__FUNCTION__,"the epoll event is already handled!");
						

					}										
				}
				
			}

			//clean the resource when worker process want to exit...
			
  			
	
		}

		//close the unix domain socket worker file descriptor;
		close(child->unix_domain_socket_fd[1]);

		//parent process
		log_to_backend(srv,MINIHTTPD_LOG_LEVEL_INFO,"worker process %d is already created!",worker_process_id);		
	}

	uint32_t main_loop=1;
    //main loop to accept client connection and re-transfter to worker process 
	while(main_loop) {

         /*
             log signal events after signal si handled by signal handler   

		 */
		if(signal_pipe_handled){
               
			
		}
		
		if(signal_child_handled){
			/*a child worker process has terminated */
			int worker_exit_status;
			pid_t exit_worker_pid;
			
			while( (exit_worker_pid= waitpid(-1,&worker_exit_status,WNOHANG))>0){
			  if(WIFEXITED(worker_exit_status)){
				  
			    log_to_backend(srv,MINIHTTPD_LOG_LEVEL_ERROR,"worker child process(pid=%d) has exited normally with exit"  \
							  "status=%d",exit_worker_pid,WEXITSTATUS(worker_exit_status));
			  }	  
			  else if(WIFSIGNALED(worker_exit_status)){
				  log_to_backend(srv,MINIHTTPD_LOG_LEVEL_ERROR,"worker child process(pid=%d) is killed by signal(%d)",
								 exit_worker_pid,WTERMSIG(worker_exit_status))  
			  }
              else{
            		 log_to_backend(srv,MINIHTTPD_LOG_LEVEL_ERROR,"worker child process(pid=%d) has exited unexpected",                                      exit_worker_pid);

			  }	 
			}
		}
		
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
 		 log_to_backend(srv,MINIHTTPD_LOG_LEVEL_INFO,"client connection is accepted,pick a worker to handle it.");
			
		 uint32_t  pick_worker_index=0;
		 uint32_t  min_sent_connections=srv->child[pick_worker_index].sent_connection_number;
         for(uint32_t worker_process_id=1; worker_process_id<srv->worker_number;worker_process_id++){
            if(srv->child[worker_process_id].sent_connection_number < min_sent_connections){
				min_sent_connections= srv->child[worker_process_id].sent_connection_number;
				pick_worker_index= worker_process_id;
			}			
		 }

		 /*set file descriptor to nonblocking and set close_on_exec flag*/
		 fd_set_nonblocking(connection_fd);
		 fd_close_on_exec(connection_fd);

		 if(unix_domain_socket_sendfd(srv->child[pick_worker_index].unix_domain_socket_fd[0],
									 connection_fd)<0){
			 log_to_backend(srv,MINIHTTPD_LOG_LEVEL_ERROR,"failed to send the connection file descriptor to worker!");
			 close(connection_fd);  //just close it to tell the client,we can not handle it now.
			 continue;			
		 }
		 srv->child[pick_worker_index].sent_connection_number++;
		 //close the file descriptor as it is already marked in flight
		 close(connection_fd);
	   }
	}

	// free all resource
	log_to_backend(srv,MINIHTTPD_LOG_LEVEL_INFO,"%s start to exit....",srv->config->service_name->ptr);


	return 0;
	
	
  	
}
