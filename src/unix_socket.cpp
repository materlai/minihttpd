/*
      unix_socket.cpp
	  
 */

#include "unix_socket.h"
#include <cassert>


/*  send the file descriptor @file_fd
	file descriptor can only be sent by unix domain socket with sendmsg
	it may block current thread when unix domain socket is not writeable and is not nonblocking

 */

int unix_domain_socket_sendfd(int unix_domain_socket_fd ,int fd)
{
    if(unix_domain_socket_fd<0 || fd<0) return -1;
    struct msghdr msg ;
	msg.msg_name=NULL;
	msg.msg_namelen=0;
	struct iovec msg_iov;
	char send_buf= ' ';
	msg_iov.iov_base =& send_buf;
	msg_iov.iov_len= sizeof(send_buf);
	msg.msg_iov= &msg_iov;
	msg.msg_iovlen=1;
	msg.msg_flags=0;

	char control_buf[CMSG_SPACE(sizeof(int))];
	msg.msg_control=control_buf;
	msg.msg_controllen=sizeof(control_buf);

	struct cmsghdr * hdr=CMSG_FIRSTHDR(&msg);
	assert(hdr!=NULL);	
    hdr->cmsg_len=CMSG_LEN(sizeof(int));
	hdr->cmsg_level=SOL_SOCKET;
	hdr->cmsg_type=SCM_RIGHTS;
    *(int*)CMSG_DATA(hdr)=fd;
	return sendmsg(unix_domain_socket_fd,&msg,0);
}


/*
     receive file descriptor from unix domain socket
 	it may block current thread when unix domain socket is not readable and is not nonblocking


 */

int unix_domain_socket_recvfd(int unix_domain_socket_fd, int * fd)
{
	if(unix_domain_socket_fd <0 || fd==NULL)   return -1;
	struct msghdr msg ;
	msg.msg_name=NULL;
	msg.msg_namelen=0;

	struct iovec msg_iov;
	char send_buf;
	msg_iov.iov_base =& send_buf;
	msg_iov.iov_len= sizeof(send_buf);
	msg.msg_iov= &msg_iov;
	msg.msg_iovlen=1;
	msg.msg_flags=0;

	char control_buf[CMSG_SPACE(sizeof(int))];
	msg.msg_control= control_buf;
	msg.msg_controllen=sizeof(control_buf);

	int n=recvmsg(unix_domain_socket_fd,&msg,0);
    if(n>0){
		struct cmsghdr * hdr= CMSG_FIRSTHDR(&msg);
		if(hdr!=NULL && hdr->cmsg_len==CMSG_LEN(sizeof(int))){
			if(hdr->cmsg_level==SOL_SOCKET && hdr->cmsg_type== SCM_RIGHTS){
				*fd= *(int*)CMSG_DATA(hdr); 				
			}
			
		}		
	}
	return n;	
}
