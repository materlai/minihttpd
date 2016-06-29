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
	if(srv->worker_number==0) srv->worker_number=1; //we have at least one worker 
	srv->child= (server_child *)malloc( sizeof(*srv->child) * srv->worker_number);
	for(uint32_t worker_index=0;worker_index<srv->worker_number;worker_index++ ){
		server_child * child= &srv->child[worker_index];
		child->sent_connection_number=0;
		child->worker_running=0;
	}		
	//set dont daemonize to false;
    srv->dont_daemonize=0;

	//server log mode(use syslog by default)
	srv->mode= server::LOG_MODE_SYSLOG;
	return srv;
}


/*free srever when minihttpd exit*/
void server_free(server* srv)
{
	assert(srv!=NULL);
	//free all child
	free((void*)srv->child);
	//free server config
	free( (void*)srv->config);

	free( (void*)srv);
	
}
