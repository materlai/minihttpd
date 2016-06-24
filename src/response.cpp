/*
    response.cpp : handle http request and prepare http response head and file data 
  
 */

#include "response.h"
#include "connection.h"
#include "worker.h"

#include <cassert>


/* initialize the response */
void response_initialize(response *r)
{
      assert(r!=NULL);
	  b->fullpath=buffer_init();
	  b->content_type=buffer_init();
	  b->content_length=0;
	
}	



/*
    prepare http request response data:
	  1) check if http_reuqets url is legal
	  2) call stat(&filename,&st) to get file size to check if the file or directory which specified by request url  exists
	  3) if the file do not exist, check if we can safely handle parse the path,if we can not, return 404 
	  3) get the content-type from mine and store it 
	  4) call modules(mod_staticfile or mod_staticdir) to handle it 
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
	if( (qstr=strstr(request_url->ptr,'#'))!=NULL){
		buffer_string_length(request_url, qstr-request_url.ptr);
	}
	if( qstr=strchar(request_url->ptr,"?")!=NULL){
		http_url_query=qstr+1;
		buffer_string_set_length(request_url, qstr-request_url->ptr);
	}

    //step 2 : get the full path of reequest url according to config->service_root_dir and request_url relative path
    buffer_copy_string(conn->connection_response.fullpath, conn->p_worker->global_config->service_root_dir);
	buffer_append_string(conn->connection_response.fullpath,"/");
	if(request_url->ptr[0]=='/')
		buffer_append_string(conn->connection_response.fullpath, request_url->ptr+1);
	else
		buffer_append_string(conn->connection_response.fullpath,request_url->ptr);
		
	
}
