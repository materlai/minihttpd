/*
     connection for per client
	 
 */

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "request.h"
#include "response.h"
#include "chunk.h"

#include <sys/socket.h>


typedef enum {
	CON_STATE_CONNECT,
	CON_STATE_REQUEST_START,
	CON_STATE_READ,
	CON_STATE_REQUEST_END,
	CON_STATE_READ_POST,
	CON_STATE_HANDLE_REQUEST,
	CON_STATE_RESPONSE_START,
	CON_STATE_WRITE,
	CON_STATE_RESPONSE_END,
	CON_STATE_ERROR,
	CON_STATE_CLOSE
} connection_state_t;


typedef struct{
	/*connection state*/
	connection_state_t status;
	
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

}connection;

#endif 
