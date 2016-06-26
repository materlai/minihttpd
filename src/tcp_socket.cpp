/*

     tcp_socekt.cpp

	
 */

#include "tcp_socket.h"
#include <sys/mman.h>

#include "buffer.h"
#include <cassert>


/*create listening tcp socket and bind to specified addr */
int create_tcp_socket(struct sockaddr *server_addr,uint32_t max_listening_number)
{
	struct sockaddr_in * addr= (struct sockaddr_in*)server_addr;
	int socket_fd= socket(addr->sin_family, SOCK_STREAM,0);

    //set reuse socket option for listening server
	int reuse_addr=1;
	setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&reuse_addr,(socklen_t)sizeof(reuse_addr));
	fd_close_on_exec(socket_fd);
	
	socklen_t addr_len=sizeof(*addr);
    if(bind(socket_fd,(struct sockaddr*)addr,addr_len)!=0){
        close(socket_fd);
		return -1;
	}

	//start to listening
	listen(socket_fd,max_listening_number);	
	return socket_fd;
}

/* it seem sendfile is not working on GCE, we use map to make one */
int tcp_sendfile(int socket_fd, int file_fd, uint32_t offset,uint32_t len)
{
      assert(socket_fd>=0 && file_fd>=0);
      const uint32_t default_page_size=4096;
	  uint32_t map_offset= offset - (offset%default_page_size);
	  uint32_t map_len= default_page_size *4;
	  if(map_len>len)  map_len=len;

	  void * map_start= mmap(NULL,map_len,PROT_READ,MAP_SHARED,file_fd, map_offset);
	  if(map_start==MAP_FAILED){
		  return -1;  //we can not mmap this map now 		  
	  }

	  buffer*b= buffer_init();
	  buffer_copy_string_length(b,(const char*)map_start,map_len);
	  //munmap is ok now
      munmap(map_start,map_len);
	  
      //send file content to socket kernel buffer;
	  assert(map_offset<=offset);
	  uint32_t send_offset= offset-map_offset;
	  uint32_t send_len= map_len-send_offset;

	  int n=write(socket_fd, b->ptr+send_offset,send_len);
	  buffer_free(b);
	  return n;	
}


