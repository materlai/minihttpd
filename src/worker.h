/*
     worker process
	 
 */

#ifndef __WORKER_H__
#define __WORKER_H__

#include "types.h"
#include "connection.h"
#include "fdevents.h"

#include <unistd.h>


typedef struct{

	/*connection */
	connection**conn;
    uint32_t conn_max_size;

    /*current connection number*/
    uint32_t  cur_connection_number;

    /*event handler for connection */
    fdevents *ev;

    /*unix domain socket file descriptor to recvive connection socket file descriptor*/
    int unix_domain_socekt_fd;
 
    /* log filepath for the worker */
    buffer * log_filepath;
    int log_fd;

}worker;


/*initialize worker*/
void worker_initialize(worker*p_worker);

#endif 
