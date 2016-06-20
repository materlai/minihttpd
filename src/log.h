/*
    warning/info/error log when  minhttpd is running,
	it only log warning/info/error after minhttpd become  daemonize process;
 */

#ifndef __LOG_H__
#define __LOG_H__


#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "buffer.h"



enum minihttpd_log_level
{
    MINIHTTPD_LOG_LEVEL_DEBUG=0,
    MINIHTTPD_LOG_LEVEL_INFO,
    MINIHTTPD_LOG_LEVEL_WARNIN,
    MINIHTTPD_LOG_LEVEL_ERROR,
};
	
/*
#define minihttpd_info(fd,format,args...) 
#define minihttpd_debug(fd,format,args...)
#define minihttpd_error(fd,format,args...)


#define minihttpd_syslog_info(fd,foramt,args...)
#define minihttpd_syslog_debug(fd,format,args...)
#define minihttpd_syslog_error(fd,format,args...)

*/
	
/*fucntion for log */
void minihttpd_running_log(int file_fd,minihttpd_log_level level,const char *filename,int line_no,const char *function_name,const char * format,...);

/*function for syslog */
void minihttpd_running_syslog(int file_fd,minihttpd_log_level level,const char *filename,int line_no,const char *function_name,const char * format,...);

#endif 
