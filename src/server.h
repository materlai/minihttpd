/*
    minihttpd server configuration 

 */

#ifndef __SERVER_H__
#define __SERVER_H__

#include "config.h"
#include "worker.h"
#include "log.h"
#include <sys/socket.h>
#include <time.h>

/*
    server_child : a worker child process for the main minihttpd process
	it should can be accessed only by main process except field: unix_domain_socket_fd 
	
 */
typedef struct {

	//pid for the worker 
	pid_t pid;
	//unix domain socket to send file descriptor between worker and server main process
	int unix_domain_socket_fd[2];
	//the connection socket file descriptor that has been sent.
	uint32_t  sent_connection_number;

}server_child;


typedef struct
{
	server_config *config; //configuration info
    /*listening socket field */
    struct sockaddr* server_addr;
	int listening_socket_fd;
	/*field about current time */
	time_t  cur_ts;
	/*field about worker process */
	uint32_t worker_number;
	
    /*worker child process:do the actually client request handle */
	server_child * child ;
   
	/*uid && gid */
	uint32_t  uid;
	uint32_t  gid;

	/*field about become daemonize process(by efault,inihttpd main process is a daemonize process) */
	uint32_t  dont_daemonize;

    enum backend_log_mode{
		LOG_MODE_SYSLOG=0,
		LOG_MODE_FILE,	   
	};
	backend_log_mode mode;
	int log_fd;   // file descriptor for log when mode=LOG_MODE_FILE;
	
}server;



/* initialize a server */
server * server_init();

/*free server resource when minhttpd is about to exit*/
void server_free(server* p_server);


/*log message to file or syslog deamon process when minihttpd service is running */
#define  log_to_backend(srv,level,format,args...)  \
	if(srv->mode==server::LOG_MODE_FILE)	 \
      minihttpd_running_log(srv->log_fd,level,__FILE__,__LINE__,__FUNCTION__,format,##args);  \
    else { minihttpd_running_syslog(srv->log_fd,level,__FILE__,__LINE__,__FUNCTION__,format,##args);  \
    }


#endif 
