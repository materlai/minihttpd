/*
     worker process
	 
 */

#ifndef __WORKER_H__
#define __WORKER_H__

#include "types.h"
#include "connection.h"
#include "fdevents.h"
#include "unix_socket.h"
#include "log.h"
#include "config.h"

#include <unistd.h>



typedef struct _worker{

    /*worker process index */
    uint32_t  worker_id;

    /*unix domain socket file descriptor to recvive connection socket file descriptor*/
    int unix_domain_socekt_fd;

	/*connection */
	connection**conn;
    uint32_t conn_max_size;
    /*current connection number*/
    uint32_t  cur_connection_number;

    /*event handler for connection */
    fdevents *ev;
 
    /* log filepath for the worker */
    buffer * log_filepath;
    int log_fd;

	/*global server config */
	server_config * global_config;
	

}worker;


/*initialize worker*/
void worker_connection_initialize(worker*p_worker,uint32_t max_connection_sizes);


/* free all conenctions */
void worker_free_connectons(worker* p_worker);


/*get a unsed connecection from worker */
connection * worker_get_new_connection(worker* srv_worker);


/* event handler when unix domain socket is readable */
int unix_domain_socket_handle(int fd, void * ctx, int events);



#endif 
