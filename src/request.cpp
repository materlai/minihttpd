/*
     request.cpp 

 */

#include "request.h"
#include "connection.h"
#include "worker.h"
#include "log.h"
#include <cstdlib>
#include <cassert>

/*initialize a request */
void request_initialize(request * r)
{
    assert(r!=NULL);
	r->request_content=buffer_init();
	r->http_method=buffer_init();
	r->request_url=buffer_init();
	r->http_version=HTTP_VERSION_UNSET;
	r->ranges=buffer_init();	
}

/* free a request */
void request_free(request *r)
{


	
}


/*
    http request head parse :
	                         http_method,request_url,http_version
							 http_head: keep_alive,ranges,....
	 return value:
	             0 for http request head parse is successfully finished.
				 -1 for incorrect http request head 
 */

int http_reuqest_parse(struct _connection * conn)
{
    char *http_request_url = NULL, *http_request_proto = NULL, *http_request_method = NULL;

	assert(conn!=NULL);
    minihttpd_running_log(conn->p_worker->log_fd, MINIHTTPD_LOG_LEVEL_INFO,
						  __FILE__,__LINE__,__FUNCTION__, "start to parse http request:%s",
						  (const char*)conn->connection_request.request_content.ptr);
	//step 1 : parse the first line: <http_method>  <http_url>  <http_version>
	uint32_t request_line_state=0;
	uint32_t offset=0;
	buffer * request_content= conn->connection_request.request_content;
	uint32_t http_reuqest_len=buffer_string_length(request_content);
	
	for(uint32_t index=0;index<http_reuqest_len;index++){
		char buf_c= request_content->ptr[index];
		switch(buf_c){
		  case ' ':{
			  switch(request_line_state){
			    case 0:{   /* http_method */
				        http_request_method= request_content->ptr + offset;
						offset=index+1;
						request_line_state++;
						break;			  
			     }
			    case 1:{   /* http request url(very important)*/
					    http_request_url= request_content->ptr+ offset;
					    offset=index+1;
						request_line_state++;
						break;		  				  
			     }
			     default: {  /* unexpected ' ' */
					 conn->http_status=400;
					 conn->keep_alive=0;
					 minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										   "http request head format is incorrect: incorrect space number!");
					 return -1;
			      }				  
			  }
			  break;
		  }

		 case '\r':{
			 if( request_content->ptr[index+1]=='\n'){
				 if(request_line_state!=2){   /*incorrect http request head */
					 conn->http_status=400;
					 conn->keep_alive=0;
					 minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										   "http request head format is incorrect!");
					 return -1;
				 }
				 //ok, we reach the end of the first line of http request
				 http_request_url= request_content->ptr+offset;
				 request_content->ptr[index]='\0';
				 request_content->ptr[index+1]='\0';

				 /* check the http_method and http_verison is legal*/
				 *(http_request_url-1)='\0';
				 *(http_request_proto-1)='\0';
                 buffer * method_b=buffer_init();
				 buffer_append_string(method_b,http_request_method);
				 http_method_t method_key=http_get_request_method(b);
				 buffer_free(method_b);
				 /*currently,we only support PUT and GET methods */
				 if(method_key!=HTTP_METHOD_GET && method_key!=HTTP_METHOD_POST) {
                     conn->http_status=501;
					 conn->keep_alive=0;
					 minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										   "unsupported http request method!");
					 return -1;					 
				 }
				 if(strncmp(http_request_proto,"HTTP/",sizeof("HTTP/")-1)==0){
					 char * major_version = http_request_proto+sizeof("HTTP/")-1;
					 char * minor_version= strstr(major_version,'.');
					 uint32_t invalid_http_version=0;
					 int major_num=0,minor_num=0;
					 if(major_version==minor_verison || minor_version==NULL
						|| *(minor_version+1)=='\0'){
						      invalid_http_version=1;						 
					 }
					 if(!invalid_http_version){
						 char * err=NULL;
						 *minor_version='\0';
						 major_num =strtol(major_version,&err,10);
						 if(*err!='\0')  invalid_http_version=1;
						 *(minor_version++)='.';
						 minor_num=strtol(minor_version,&err,10);
						 if(*err!='\0') invalid_http_version=1;
						 
					 }
					 /*  http version format is incorrect  */
					 if(invalid_http_version){
						 conn->http_status=400;
					     conn->keep_alive=0;
					     minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										   "http protocol version information format is incorrect!");
					     return -1;						 
					 }

					 if(major_num==1 && minor_num==1)
						 conn->connection_request.http_version= conn->p_worker->global_config->support_http_1_1? HTTP_VERSION_1_1:HTTP_VERSION_1_0;
					 else if(major_num==1 && minor_num==0){
						 conn->connection_request.http_version=HTTP_VERSION_1_0;						 
					 }else{

						 conn->http_status=400;
					     conn->keep_alive=0;
					     minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										   "unsupported http protocol version");
					     return -1;						
						 
					 }
					 
					 
					  
				 }else{
                         conn->http_status=400;
					     conn->keep_alive=0;
					     minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										  "http protocol version head format is incorrect!");
					     return -1;	
				 }				 
			 }			
		  }
		 default:{


			
		 }
	   }
		
	}
		
  
	

	
}



/* get http request method from buffer @b  */
http_method_t http_get_request_method(buffer *b)
{
   assert(b!=NULL);
   typedef struct {
		http_method_t key;
		const char * value; 
   }keyvalue;

   static keyvalue http_methods[] = {
	{ HTTP_METHOD_GET, "GET" },
	{ HTTP_METHOD_HEAD, "HEAD" },
	{ HTTP_METHOD_POST, "POST" },
	{ HTTP_METHOD_PUT, "PUT" },
	{ HTTP_METHOD_DELETE, "DELETE" },
	{ HTTP_METHOD_CONNECT, "CONNECT" },
	{ HTTP_METHOD_OPTIONS, "OPTIONS" },
	{ HTTP_METHOD_TRACE, "TRACE" },
	{ HTTP_METHOD_ACL, "ACL" },
	{ HTTP_METHOD_BASELINE_CONTROL, "BASELINE-CONTROL" },
	{ HTTP_METHOD_BIND, "BIND" },
	{ HTTP_METHOD_CHECKIN, "CHECKIN" },
	{ HTTP_METHOD_CHECKOUT, "CHECKOUT" },
	{ HTTP_METHOD_COPY, "COPY" },
	{ HTTP_METHOD_LABEL, "LABEL" },
	{ HTTP_METHOD_LINK, "LINK" },
	{ HTTP_METHOD_LOCK, "LOCK" },
	{ HTTP_METHOD_MERGE, "MERGE" },
	{ HTTP_METHOD_MKACTIVITY, "MKACTIVITY" },
	{ HTTP_METHOD_MKCALENDAR, "MKCALENDAR" },
	{ HTTP_METHOD_MKCOL, "MKCOL" },
	{ HTTP_METHOD_MKREDIRECTREF, "MKREDIRECTREF" },
	{ HTTP_METHOD_MKWORKSPACE, "MKWORKSPACE" },
	{ HTTP_METHOD_MOVE, "MOVE" },
	{ HTTP_METHOD_ORDERPATCH, "ORDERPATCH" },
	{ HTTP_METHOD_PATCH, "PATCH" },
	{ HTTP_METHOD_PROPFIND, "PROPFIND" },
	{ HTTP_METHOD_PROPPATCH, "PROPPATCH" },
	{ HTTP_METHOD_REBIND, "REBIND" },
	{ HTTP_METHOD_REPORT, "REPORT" },
	{ HTTP_METHOD_SEARCH, "SEARCH" },
	{ HTTP_METHOD_UNBIND, "UNBIND" },
	{ HTTP_METHOD_UNCHECKOUT, "UNCHECKOUT" },
	{ HTTP_METHOD_UNLINK, "UNLINK" },
	{ HTTP_METHOD_UNLOCK, "UNLOCK" },
	{ HTTP_METHOD_UPDATE, "UPDATE" },
	{ HTTP_METHOD_UPDATEREDIRECTREF, "UPDATEREDIRECTREF" },
	{ HTTP_METHOD_VERSION_CONTROL, "VERSION-CONTROL" },

	{ HTTP_METHOD_UNSET, NULL }
   };

   for(uint32_t index=0;http_methods[index].value!=NULL; index++)
	   if(strcmp((const char*)b->ptr, http_methods[index].value)==0)
		   return http_methods[index].key;
   
   return HTTP_METHOD_UNSET;   
}
