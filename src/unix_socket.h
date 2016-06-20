/*
   unix domain socket: send the file descriptor to other process  

 */

#ifndef __UNIX_SOCKET_H__
#define __UNIX_SOCKET_H__

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/socket.h>

/*  send the file descriptor @file_fd
	file descriptor can only be sent by unix domain socket with sendmsg 

 */

int unix_domain_socket_sendfd(int unix_domain_socket_fd ,int file_fd);


/*
     receive file descriptor from unix domain socket

 */

int unix_domain_socket_recvfd(int unix_domain_socket_fd, int * fd);


#endif 
