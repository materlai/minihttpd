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
	CON_STATE_CLOSE,
	CON_STATE_CONNECT,
} connection_state_t;

struct _worker;

typedef struct _connection{

   /*conection index in worker process */	
	uint32_t connection_index;
	
	/*connection state*/
	connection_state_t state;	
	/*file descriptors for connection socket */
	int conn_socket_fd;
	struct sockaddr client_addr;

     /* worker */
	struct _worker * p_worker;  /*the worker that handle the connection */

	/* timestamp for the connection */
	time_t  start_ts;
	/* timestamp about last successfully read/write */
	time_t active_read_ts;
	time_t active_write_ts;
		
	time_t  close_timeout_ts;  /* the time we sent out FIN */

	/*  below filed is about http request and response */
	
	/*readable/writeable */
	uint32_t readable;
	uint32_t writeable;

	/*connection keep-alive flag*/
	uint32_t keep_alive;

    /*http status  after http handle*/
	uint32_t http_status;

	/* http_response_body  is ready now  */
    uint32_t  http_response_body_finished;
	
	/*read/write chunkqueue */
	chunkqueue * readqueue;
	chunkqueue *writequeue;

	/*request and response*/
	request connection_request;
	response connection_response;
}connection;



/* initialize connecitonn*/
void connection_set_default(connection * conn);


/* reset connection to default state */
void connection_reset(connection* conn);


/* close the connection */
void connection_close(connection * conn);

/*  free connection resource */
void connection_free(connection * conn);
	
	
/* update state for connection  */
void connection_set_state(connection*conn,connection_state_t state);

/*event handler for connection file descriptor */
int connection_event_handle(int fd, void *handler_ctx,int events);

/* connection socket event handle state machine  */
int connection_state_machine(connection * conn);


/* handle conneciton socket read request   */
int connection_handle_read_state(connection * conn);


/*prepare http response head field ( content-length/content-type,etc.... ) */
int connection_handle_prepare_response_head(connection * conn);

/*send http response status and http response head to client  */
int connection_write_response_head(connection* conn);




/*  handle connection write state */
int connection_handle_write_state(connection * conn);




#endif 


