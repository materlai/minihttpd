
#include "daemonize.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/resource.h>
#include <syslog.h>

#include <cstdio>
#include <cstdlib>

int daemonize(const char* service_name)
{
	umask(0);
	//step 1 : fork a child process
	pid_t child_pid=fork();
	if(child_pid<0){
		fprintf(stderr,"[%s|%d|%s]: failed to call fork()\n",__FILE__,__LINE__,__FUNCTION__);
		return -1;		
	}
	else if(child_pid>0){
		exit(0);      //parent process exit 		
	}
	setsid();
    //step2 : fork a child process and make session control process(current process exit);
	struct sigaction  sig;
	sig.sa_handler=SIG_IGN;
	sig.sa_flags=0;
	sigemptyset(&sig.sa_mask);
	sigaction(SIGHUP,&sig,NULL);

	child_pid=fork();
	if(child_pid<0){
		return -1;		
	}
	else if(child_pid>0) {
		exit(0);  //terminated current process 		
	}
	//we got the process we need
	//step 3 :close  all file descriptor
	struct  rlimit res_limit;
	if(getrlimit(RLIMIT_NOFILE,&res_limit)!=0){
		   exit(-1);		
	}
	for(int index=0;index<res_limit.rlim_cur;index++)
		close(index);
	int file_fd=open("/dev/null",O_RDWR);
	dup2(file_fd,STDOUT_FILENO);
	dup2(file_fd,STDERR_FILENO);

	//step 4: change working directory to /
	chdir("/");

	//step 5 : open syslog for log message
	openlog(service_name, LOG_CONS | LOG_PID, LOG_DAEMON);
	return 0;
}
