/*
      daemon process 

 */

#ifndef __DAEMONIZE_H__
#define __DAEMONIZE_H__

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


//become a daemon process
int  daemonize(const char* service_name);




#endif 
