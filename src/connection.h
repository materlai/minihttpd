/*
     connection for per client
	 
 */

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "request.h"
#include "response.h"
#include "chunk.h"

#include <sys/socket.h>
#include <time.h>


typedef enum {
	CON_STATE_REQUEST_START=0,
	CON_STATE_REQUEST_END,
	CON_STATE_READ,
	CON_STATE_HANDLE_REQUEST,
	CON_STATE_RESPONSE_START,
	CON_STATE_WRITE,
	CON_STATE_RESPONSE_END,
	CON_STATE_ERROR,
	CON_STATE_CLOSE
} connection_state_t;

struct _worker;

typedef struct _connection{
	/*connection state*/
	connection_state_t state;
	
	/*filed about connection socket */
	int conn_socket_fd;
	struct sockaddr client_addr;

	/*readable/writeable */
	uint32_t readable;
	uint32_t writeable;

	/*connection keep-alive flag*/
	uint32_t keep_alive;

    /*http status  after http handle*/
	uint32_t http_status;

	/*read/write chunkqueue */
	chunkqueue * readqueue;
	chunkqueue *writequeue;

	/*request and response*/
	request connection_request;
	response connection_response;

	/* worker */
	struct _worker * p_worker;  /*the worker that handle the connection */

	/* timestamp for the connection */
	time_t  start_ts;
	time_t  end_ts;

}connection;


/* update state for connection  */
void connection_set_state(connection*conn,connection_state_t state);

/*event handler for connection file descriptor */
int connection_event_handle(int fd, void *handler_ctx,int events);

/* connection socket event handle state machine  */
int connection_state_machine(connection * conn);


/* handle conneciton socket read request   */
int connection_handle_read_state(connection * conn);

/*  handle connection write state */
int connection_handle_write_state(connection * conn);




#endif 


