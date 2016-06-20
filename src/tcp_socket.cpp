/*

     tcp_socekt.cpp

	
 */

#include "tcp_socket.h"


/*create listening tcp socket and bind to specified addr */
int create_tcp_socket(struct sockaddr *server_addr,uint32_t max_listening_number)
{
	struct sockaddr_in * addr= (struct sockaddr_in*)server_addr;
	int socket_fd= socket(addr->sin_family, SOCK_STREAM,0);
	socklen_t addr_len=sizeof(*addr);
    if(bind(socket_fd,(struct sockaddr*)addr,addr_len)!=0){
        close(socket_fd);
		return -1;
	}
	//set reuse socket option for listening server
	int reuse_addr=1;
	setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&reuse_addr,(socklen_t)sizeof(reuse_addr));
	fd_close_on_exec(socket_fd);

	//start to listening
	listen(socket_fd,max_listening_number);
	
	return socket_fd;
}


