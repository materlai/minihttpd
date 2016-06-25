/*
    response.cpp : handle http request and prepare http response head and file data 
  
 */

#include "response.h"
#include "connection.h"
#include "worker.h"
#include "key_map.h"

#include <dirent.h>
#include <errno.h>

#include <cassert>



/* initialize the response */
void response_initialize(response *r)
{
      assert(r!=NULL);
	  r->fullpath=buffer_init();
	  r->content_type=buffer_init();
	  r->content_length=0;
}	

/*
    prepare http request response data:
	  1) check if http_reuqets url is legal
	  2) call stat(&filename,&st) to get file size to check if the file or directory which specified by request url  exists
	  3) if the file do not exist, check if we can safely handle parse the path,if we can not, return 404 
	  3) get the content-type from mine and store it 
	  4) call modules(mod_staticfile or mod_staticdir) to handle it

	  return value:  0  for a sucessfully http request
	                 -1 for invalid http request(persmission, target file do not exist,etc...)
	  
 */

int  http_request_handle(struct _connection * conn)
{
	char * qstr=NULL;
	char * http_url_query=NULL;
	assert(conn!=NULL && conn->p_worker!=NULL);

	if(conn->http_status!=0 && conn->http_status!=200){
		/* someone has handle the http request */
		return 0;
	}

	//step 1: check the http request url is valid path
	buffer * request_url= conn->connection_request.request_url;
	assert(request_url!=NULL);
	if( (qstr=strchr((char*)request_url->ptr,'#'))!=NULL){
	    buffer_string_set_length(request_url, qstr-(char*)request_url->ptr);
	}
	if( ( qstr=strchr((char*)request_url->ptr,'?') ) !=NULL){
		http_url_query=qstr+1;
		buffer_string_set_length(request_url, qstr-(char*)request_url->ptr);
	}

    //step 2 : get the full path of reequest url according to config->service_root_dir and request_url relative path
    buffer_copy_buffer(conn->connection_response.fullpath, conn->p_worker->global_config->service_root_dir);
	buffer_append_string(conn->connection_response.fullpath,"/");
	if(request_url->ptr[0]=='/')
		buffer_append_string(conn->connection_response.fullpath, (const char*)request_url->ptr+1);
	else
		buffer_append_string(conn->connection_response.fullpath,(const char*)request_url->ptr);

	//step 3 : check if the target file exists
	struct stat s_stat;
	if(stat((const char*)conn->connection_response.fullpath->ptr,&s_stat)!=0){
		/*can not stat the taget file(the file do not exist or we do not have permisson to access it) */
		switch(errno){
		 case EACCES:
			          conn->http_status=403;
			          buffer_reset(conn->connection_response.fullpath);
					  break;
		case ENOENT:
			          conn->http_status=404;
			          buffer_reset(conn->connection_response.fullpath);
					  break;
		default:
			   /*  the other reason cause we can not access the target file  */
			  conn->http_status=500;
			  buffer_reset(conn->connection_response.fullpath);
			  break;						
		}		
		return -1;
	}

	//if target file is regular file  
	if(S_ISREG(s_stat.st_mode)){

   	   /* identify the content-type of http response from filename extension */
   	   mime_table * table =conn->p_worker->global_config->table;
 	   assert(table!=NULL);

       uint32_t fullpath_len=buffer_string_length(conn->connection_response.fullpath);
	   uint32_t index=0;
	   for(;index<table->used_entry;index++){
		   mime_type_entry * entry= table->entry[index];

		   if(strlen(entry->extension)<=0)  continue;
		   if(strlen(entry->extension)> fullpath_len)  continue;
     
		   if(strncasecmp((char*)conn->connection_response.fullpath->ptr+fullpath_len-strlen(entry->extension),
					                          entry->extension,strlen(entry->extension) )==0 ){
		    	/*  we get a match */
			   buffer_copy_string(conn->connection_response.content_type,entry->mime_type);
			   break;			
	 	  }		
	  }

	  /* append the file to writequeue  */
	   chunkqueue_append_file(conn->writequeue,conn->connection_response.fullpath,
							  0,s_stat.st_size);
	   memcpy(&conn->connection_response.s_stat,&s_stat,sizeof(s_stat));
	   conn->http_response_body_finished=1;	
	   conn->http_status=200;
		
	}else if(S_ISDIR(s_stat.st_mode)){

        /*if target file is directory */
		buffer * b= http_handle_directory_parse(conn, conn->connection_response.fullpath);
		assert(b!=NULL);
#if 1
        minihttpd_running_log(conn->p_worker->log_fd, MINIHTTPD_LOG_LEVEL_INFO,
							  __FILE__,__LINE__,__FUNCTION__,"%s",(const char*)b->ptr);
		
#endif 		
		chunkqueue_append_buffer(conn->writequeue,b);
        buffer_free(b);

		memcpy(&conn->connection_response.s_stat,&s_stat,sizeof(s_stat));
	    conn->http_response_body_finished=1;	
	    conn->http_status=200;		
	}
	else{
		/*we can only allow access to regular and directory  */
	    conn->http_status=403;
		buffer_reset(conn->connection_response.fullpath);
		return -1;		
	}
	return 0;
}


/* read directory content and return buffer which contain the directory entry*/
buffer * http_handle_directory_parse(struct _connection * conn,  buffer*directory_path )
{
	assert( directory_path!=NULL);
	if( buffer_is_empty(directory_path) )    return NULL;

	/*
	    read the directory entry and append entry to buffer with following format:
	    format:  filename     last_modified_time    filesizes

	*/
    buffer * b= buffer_init();
    //step 1 : append "html and head/title to buffer "
	buffer_append_string(b,"<html>\r\n");
	buffer_append_string(b,"<head><title>");
	buffer_append_string(b,"Index of ");
	buffer_append_buffer(b,conn->connection_request.request_url);
	buffer_append_string(b,"</title></head>\r\n");
	//step2 : append body indicator to buffer
	buffer_append_string(b,"<body bgcolor=\"white\">\r\n");
	buffer_append_string(b,"<h1>Index of ");
	buffer_append_string(b,(const char*)conn->connection_request.request_url->ptr);
	buffer_append_string(b,"</h1><hr><pre>\r\n");

	//step 3 : append every file with format (<a href="filename"> filename </a> )to buffer
	DIR * dir=opendir( (const char*)directory_path->ptr);
	if(dir){
		
		struct dirent * entry=NULL;
		while( (entry=readdir(dir))!=NULL) {

		  if(strcmp(entry->d_name,".")==0)  continue;

		  /* call stat() to get file/directory size */
		  buffer * fullpath= buffer_init();
		  buffer_copy_buffer(fullpath,directory_path);
		  if(fullpath->ptr[buffer_string_length(fullpath)-1]!='/')
			  buffer_append_string(fullpath,"/");
		  buffer_append_string(fullpath,entry->d_name);
		  struct stat s_stat;
		  if(stat((const char*)fullpath->ptr,&s_stat)!=0) {
			  /* we can not access the file or directory */
			  buffer_free(fullpath);
			  continue;
		  }
	      buffer_free(fullpath);

          /* append file name   */			
		  buffer *filename_html =buffer_init();
		  buffer_append_string(filename_html,"<a href=\"");
		  buffer_append_string(filename_html, entry->d_name);
		  buffer_append_string(filename_html,"\">");
		  buffer_append_string(filename_html, entry->d_name);
		  buffer_append_string(filename_html,"</a>");

		  /* prepare file last modified time */
		  buffer * last_modified_time= buffer_init();
		  buffer_alloc_size(last_modified_time,256);
		  strftime((char*)last_modified_time->ptr, last_modified_time->size,
				      "%Y-%m-%d %H:%M:%S",localtime(&s_stat.st_mtime));

		  /*prepare  file or directory sizes  */
          buffer * filesize=buffer_init();
		  buffer_alloc_size(filesize,32);
		  snprintf((char*)filesize->ptr,filesize->size,"%d",s_stat.st_size);
		  
		  /* append name/the last modified time/sizes of file or directory */
		  const uint32_t max_line_sizes=1024;
		  char * html_line = (char*)malloc(sizeof(char)*max_line_sizes);
		  memset(html_line,0, max_line_sizes);

		  snprintf(html_line, max_line_sizes,  "%-64s",(const char*)filename_html->ptr);
		  snprintf(html_line+strlen(html_line),max_line_sizes-strlen(html_line), "%32s",
				                                     (const char*)last_modified_time->ptr);
		  snprintf(html_line+strlen(html_line), max_line_sizes-strlen(html_line), "%32s\r\n",
				                                 (const char*)filesize->ptr);

		  buffer_append_string(b, html_line);

		  buffer_free(filename_html);
		  buffer_free(last_modified_time);
		  buffer_free(filesize);
		  free( (void*)html_line );		  
	   }				
	}
	//append the end of html text to buffer
	buffer_append_string(b,"</pre><hr></body>\r\n");
	buffer_append_string(b,"</html>\r\n");
	return b;
}
