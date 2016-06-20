/*
    server.cpp: initialize sever and free server resource  

 */


#include "server.h"
#include <cassert>

/*initialize server*/
server * server_init()
{
    server * srv= (server*)malloc(sizeof(*srv));
	memset(srv,0,sizeof(server));
    //step1: initialize global all fields
	srv->config=server_config_init();
	set_config_default(srv->config);
	assert(srv->config!=NULL);
    //initialize server start time;
	time(&srv->cur_ts);
	//initialize worker
	srv->worker_number=srv->config->max_worker_number;
	srv->p_worker=(worker*)malloc( srv->worker_number*sizeof(*srv->p_worker) );
	for(int index=0;index<srv->worker_number;index++)
		worker_initialize(&srv->p_worker[index]);
	srv->unix_domain_socket_fd= (int*)malloc(srv->worker_number*sizeof(int));
	srv->worker_pid= (pid_t*) malloc( srv->worker_number * sizeof(*srv->worker_pid));
	srv->sent_connection_number= (uint32_t*) malloc( srv->worker_number* sizeof(*srv->sent_connection_number) );

	//set dont daemonize to false;
    srv->dont_daemonize=0;

	//server log mode(use syslog by default)
	srv->mode= server::LOG_MODE_SYSLOG;
	return srv;
}


/*free srever when minihttpd exit*/
void server_free(server* srv)
{
   
  	
}
