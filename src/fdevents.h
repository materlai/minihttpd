/*
    fdevent: event handler
	

 */
#ifndef __FDEVENTS_H__
#define __FDEVENTS_H__

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

typedef  int (*event_handle)(int fd, void *handler_ctx,int events);

typedef struct {
	int fd;
	void *handler_ctx;
	event_handle  handler;
	int events ;  //the events we care 	
}fdnodes;


typedef struct{

	int max_fd;   //max file descriptor number we used in process	
	fdnodes **hanlder_array;
	int epoll_fd;
	struct epoll_event *epoll_events;
	uint32_t max_epoll_events;
}fdevents;



/*initialize fdevent */
fdevents* fdevent_initialize(int max_fd);

/*free fdevents*/
void fdevent_free(fdevents* ev);



/*register event on file descriptor */
void fdevents_register_fd(fdevents * ev, int fd, event_handle handler,void * ctx);


/*unregister event handler on file descriptor  */
void fdevents_unregister_fd(fdevents* ev,int fd);



/*get file descriptor handler when events occurs */
event_handle  fdevents_get_events(fdevents * ev, int fd);


/*get file descriptor handle context when events occurs*/
void* fdevents_get_context(fdevents *ev,int fd);




/*close the file descriptor when exec  */
void fd_close_on_exec(int fd);


/*set file descriptor to nonblocking*/
void fd_set_nonblocking(int fd);

/*set file descriptor to blocking */
void fd_clear_nonblocking(int fd);




/* event handler when unix domain socket is readable */
int unix_domain_socket_handle(int fd, void * ctx, int events);


/*set file descriptor events */
int fdevents_set_events(fdevents*ev,int fd, int events);

/*unset file descriptor events*/
int fdevents_unset_event(fdevents*ev, int fd);



#endif 
