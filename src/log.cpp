/*
   log.cpp : log debug/info/warning/error message to file to syslog 
 */

#include "log.h"
#include <time.h>
#include <cstdarg>
#include <cassert>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>


/*
   message format:
   DATE(YY-MM-DD-HH-MM-SS)- filename_line_no_function_name - level: string content   

*/

buffer * minihttp_prepare_log(minihttpd_log_level level, const char *filename, uint32_t line_no,
							   const char *function_name)
{
	buffer * b=buffer_init();      
	buffer * local_time_buffer=buffer_init();
	buffer_alloc_size(local_time_buffer,256);
	time_t ts;
	time(&ts);
	strftime((char*)local_time_buffer->ptr,local_time_buffer->size,"%Y-%m-%d %H:%M:%S",localtime(&ts));
	local_time_buffer->used_bytes= strlen((const char*)local_time_buffer->ptr)+1;
    local_time_buffer->ptr[local_time_buffer->used_bytes-1]='\0';  
	buffer_copy_buffer(b,local_time_buffer);  
    buffer_free(local_time_buffer); 
   
	buffer_append_string(b," - ");  
	buffer_append_string(b,filename);

    buffer_append_string(b,"_");
	char  log_line_no[32]; 
	snprintf(log_line_no,sizeof(log_line_no),"%d",line_no); 
	buffer_append_string(b,log_line_no); 

	buffer_append_string(b,"_"); 
	buffer_append_string(b,function_name);  

	const char*  log_level_status[]={"-DEBUG","-INFO","-WARNING","-ERROR"}; 
	assert(level >= 0 && level < sizeof(log_level_status)/sizeof(log_level_status[0]));
	buffer_append_string(b,log_level_status[level]);
	buffer_append_string(b,":");

	return b;
}

/*fucntion for log */
void minihttpd_running_log(int file_fd, minihttpd_log_level level,const char *filename,int line_no,const char *function_name,const char * format,...)
{
    buffer *b=minihttp_prepare_log(level,filename,line_no,function_name);
	assert(b!=NULL);

    buffer* format_content=buffer_init();  
	buffer_alloc_size(format_content,1024); 
	va_list arg; 
	va_start(arg,format);  
	vsprintf((char*)format_content->ptr,format,arg);
	format_content->used_bytes= strlen((const char*)format_content->ptr)+1;   
	va_end(arg);
	
	buffer_append_string(b,(const char*)format_content->ptr);
	buffer_append_string(b,"\n");
 	buffer_free(format_content);

	uint8_t * buf=b->ptr;
	uint32_t buf_length=buffer_string_length(b);
	//write all log to file
	if(file_fd>=0){
		  while(buf_length>0){
			  int n=write(file_fd,buf,buf_length);
			  if(n<0){
                     switch(errno){
				       case EINTR:
					              break; // retry again
				       default:  return;  //we meet a  error which should not happen	   
				     }			   
			  }else if(n==0) {

				   // should not happen,but we continue here
				  
			  }else{
				       buf+=n;
					   buf_length-=n;			  				  
			  }             			
	      }		
	}

	buffer_free(b);
}

/*function for syslog */
void minihttpd_running_syslog(int file_fd, minihttpd_log_level level,const char *filename,int line_no,const char *function_name,const char * format,...)
{

	buffer * b=minihttp_prepare_log(level,filename,line_no,function_name);

    buffer* format_content=buffer_init();  
	buffer_alloc_size(format_content,1024); 

	va_list arg; 
	va_start(arg,format);  
	vsprintf((char*)format_content->ptr,format,arg);
	format_content->used_bytes=strlen((const char*)format_content->ptr)+1;

	
	va_end(arg);   
    buffer_append_string(b,(const char*)format_content->ptr);
 	buffer_free(format_content);

	syslog(LOG_INFO, "%s",b->ptr);

	buffer_free(b);
}

