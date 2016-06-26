/*
    fdevents.cpp
	

 */

#include "fdevents.h"
#include <cstdlib>
#include <cstring>
#include <cassert>


/*initialize fdevent */
fdevents* fdevent_initialize(int max_fd)
{
	fdevents * ev= (fdevents*)malloc(sizeof(*ev));
	memset(ev,0,sizeof(*ev));
	ev->max_fd=max_fd;
	ev->max_epoll_events=max_fd;
	ev->epoll_fd=epoll_create(ev->max_fd);
	if(ev->epoll_fd<0){
		free((void*)ev);
		return NULL;
	}
	ev->hanlder_array= (fdnodes**)malloc(ev->max_fd*sizeof(*ev->hanlder_array));
	memset(ev->hanlder_array,0,sizeof(*ev->hanlder_array)*ev->max_fd);

	ev->epoll_events=(struct epoll_event*)malloc( ev->max_epoll_events * sizeof(struct epoll_event) );

	return ev;
	
}

/*free fdevents*/
void fdevent_free(fdevents* ev)
{

	if(ev){
		free((void*)ev->epoll_events);
		free((void*)ev->hanlder_array);
		free((void*)ev);	   
	}
	
}



/*register event handler  on file descriptor */
void fdevents_register_fd(fdevents * ev, int fd, event_handle handler,void * ctx)
{

	assert(ev !=NULL && handler!=NULL && ctx!=NULL);
	assert(ev->hanlder_array[fd]==NULL);
	assert(fd>=0 &  fd<ev->max_fd );
	ev->hanlder_array[fd]= (fdnodes*) malloc(sizeof(fdnodes))  ;
	ev->hanlder_array[fd]->fd=fd;
	ev->hanlder_array[fd]->handler= handler;
	ev->hanlder_array[fd]->handler_ctx=ctx;
	ev->hanlder_array[fd]->events=0;
		
}


/*unregister event handler on file descriptor  */
void fdevents_unregister_fd(fdevents* ev,int fd)
{
	  assert(ev!=NULL);
	  assert(fd>=0 && fd<ev->max_fd);
	  assert(ev->hanlder_array[fd]!=NULL);


	  fdnodes * fd_nodes= ev->hanlder_array[fd];
	  if(fd_nodes){
		  free((void*)fd_nodes);		  
	  }
	  ev->hanlder_array[fd]=NULL;	  
	
}



/*set file descriptor events */
int fdevents_set_events(fdevents*ev,int fd, int events)
{

	assert(ev!=NULL);
	assert(fd>=0 && fd<ev->max_fd);
	assert(ev->hanlder_array[fd]!=NULL);
	fdnodes * fd_node=ev->hanlder_array[fd];
	struct epoll_event event;
	event.events= events | EPOLLERR | EPOLLHUP ;
	event.data.fd=fd;
	epoll_ctl(ev->epoll_fd, fd_node->events==0?EPOLL_CTL_ADD:EPOLL_CTL_MOD,
			  fd,&event);
	fd_node->events=events;	
}

/*unset file descriptor events*/
int fdevents_unset_event(fdevents*ev, int fd)
{
    assert(ev!=NULL);
	assert(fd>=0 && fd<ev->max_fd);
	assert(ev->hanlder_array[fd]!=NULL);

	fdnodes * fd_node= ev->hanlder_array[fd];
	struct epoll_event event;
	if(fd_node->events!=0)
		epoll_ctl(ev->epoll_fd,EPOLL_CTL_DEL,fd,&event);	
    fd_node->events=0;

}


/*get file descriptor handler when events occurs */
event_handle  fdevents_get_handle(fdevents * ev, int fd)
{

	assert(ev!=NULL);
	assert(fd>=0 && fd<ev->max_fd);
	assert(ev->hanlder_array[fd]!=NULL);
	return  ev->hanlder_array[fd]->handler;
}


/*get file descriptor handle context when events occurs*/
void* fdevents_get_context(fdevents *ev,int fd)
{
  	assert(ev!=NULL);
	assert(fd>=0 && fd<ev->max_fd);
	assert(ev->hanlder_array[fd]!=NULL);
	return ev->hanlder_array[fd]->handler_ctx;	
}



/*close the file descriptor when exec  */
void fd_close_on_exec(int fd)
{

	int fd_flags= fcntl(fd,F_GETFD,0);
	fd_flags |= FD_CLOEXEC;
	fcntl(fd,F_SETFD,fd_flags);

}


/*set file descriptor to nonblocking*/
void fd_set_nonblocking(int fd)
{

	int fd_flags=fcntl(fd,F_GETFL,0);
	fd_flags |=O_NONBLOCK;
	fcntl(fd,F_SETFL,fd_flags);  
	
}

/*set file descriptor to blocking */
void fd_clear_nonblocking(int fd)
{

  	int fd_flags=fcntl(fd,F_GETFL,0);
	fd_flags &= ~O_NONBLOCK;
	fcntl(fd,F_SETFL,fd_flags);  
	
}








