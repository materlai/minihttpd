/*
    response.cpp : handle http request and prepare http response head and file data 
  
 */

#include "response.h"
#include "connection.h"
#include "worker.h"
#include "key_map.h"

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

int response_http_prepare(struct _connection * conn)
{
	char * qstr=NULL;
	char * http_url_query=NULL;
	assert(conn && conn->p_worker!=NULL);

	buffer_reset(conn->connection_response.content_type);
    conn->connection_response.content_length=0;
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

	if( S_ISLNK(s_stat.st_mode) )  {
		/*we can not allow access to syslink */
	    conn->http_status=403;
		buffer_reset(conn->connection_response.fullpath);
		return -1;		
 	}

	memcpy(&conn->connection_response.s_stat,&s_stat,sizeof(s_stat));
	/*
        target file exists and we can access it
		 1) prepare target file size from stat_st_size
		 2) identify the content-type of http response from filename extension
	 */
	conn->connection_response.content_length= s_stat.st_size;

	mime_table * table ;
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
	/*if we do not get a match extension */
	if(index==table->used_entry)
		buffer_copy_string(conn->connection_response.content_type,"application/octet-stream");

	/* append the file to writequeue  */
	
}
