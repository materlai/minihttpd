/*
    tcp socket 
 */

#ifndef __TCP_SOCKET_H__
#define  __TCP_SOCKET_H__
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "fdevents.h"

/*create listening tcp socket and bind to specified addr */
int create_tcp_socket(struct sockaddr *addr,uint32_t max_listening_number);



#endif 
