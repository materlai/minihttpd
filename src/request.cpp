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
	r->hostname=buffer_init();
	r->http_range=buffer_init();
	r->keep_alive=HTTP_CONNECTION_UNSET;  /**/
}


/*  request reset */
void request_reset(request *r)
{
	assert(r!=NULL);
	buffer_reset(r->request_content);
	buffer_reset(r->http_method);
	buffer_reset(r->request_url);
	r->http_version=HTTP_VERSION_UNSET;
	r->keep_alive= HTTP_CONNECTION_UNSET;
	buffer_reset(r->hostname);
	buffer_reset(r->http_range);   	
}


/* free a request */
void request_free(request *r)
{
	assert(r!=NULL);
	buffer_free(r->request_content);
	buffer_free(r->http_method);
	buffer_free(r->request_url);
	buffer_free(r->http_range);
	buffer_free(r->hostname);   	
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
						  (const char*)conn->connection_request.request_content->ptr);
	//step 1 : parse the first line: <http_method>  <http_url>  <http_version>
	uint32_t request_line_state=0;
	uint32_t offset=0;
	uint32_t end_of_line=0;
	buffer * request_content= conn->connection_request.request_content;
	uint32_t http_reuqest_len=buffer_string_length(request_content);
	uint32_t index=0;
	for(;index<http_reuqest_len && end_of_line==0 ;index++) {
		char buf_c= request_content->ptr[index];
		switch(buf_c){
		  case ' ':{
			  switch(request_line_state) {
			    case 0:{   /* http_method */
					    http_request_method= (char*)request_content->ptr + offset;
						offset=index+1;
						request_line_state++;
						break;			  
			     }
			    case 1:{   /* http request url(very important)*/
					    http_request_url=  (char*)request_content->ptr+ offset;
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
			 if( request_content->ptr[index+1]=='\n') {  /*we found the end of the first line */
				 if(request_line_state!=2){   /*incorrect http request head */
					 conn->http_status=400;
					 conn->keep_alive=0;
					 minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										   "http request head format is incorrect!");
					 return -1;
				 }
				 //ok, we reach the end of the first line of http request
				 http_request_proto  = (char*)request_content->ptr+offset;
				 request_content->ptr[index]='\0';
				 request_content->ptr[index+1]='\0';

				 /* check the http_method and http_verison is legal*/
				 *(http_request_url-1)='\0';
				 *(http_request_proto-1)='\0';
				 
                 buffer * method_b=buffer_init();
				 buffer_append_string(method_b,http_request_method);
				 http_method_t method_key=http_get_request_method(method_b);				 
				 /*currently,we only support PUT and GET methods */
				 if(method_key!=HTTP_METHOD_GET && method_key!=HTTP_METHOD_POST) {
                     conn->http_status=501;
					 conn->keep_alive=0;
					 minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										   "unsupported http request method!");
					 return -1;					 
				 }
                 buffer_copy_buffer(conn->connection_request.http_method,method_b);
				 buffer_free(method_b);

				 /*copy user request url, we will check the url if is it legal later */
				 buffer_copy_string(conn->connection_request.request_url,http_request_url);				 

				 /* check http version of http request is supported  */
				 if(strncmp(http_request_proto,"HTTP/",sizeof("HTTP/")-1)==0) {
					 char * major_version = http_request_proto+sizeof("HTTP/")-1;
					 char * minor_version= strchr(major_version,'.');
					 uint32_t invalid_http_version=0;
					 int major_num=0,minor_num=0;
					 if( minor_version==NULL || major_version==minor_version
						 ||    *(minor_version+1)=='\0'){
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

					 if(major_num==1 && minor_num==1){
					   conn->connection_request.http_version= conn->p_worker->global_config->support_http_1_1?
						                                               HTTP_VERSION_1_1:HTTP_VERSION_1_0;
					 }
					 else if(major_num==1 && minor_num==0){
						 conn->connection_request.http_version=HTTP_VERSION_1_0;						 
					 }else{

						 conn->http_status=400;
					     conn->keep_alive=0;
					     minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										   "unsupported http protocol version");
					     return -1;						
						 
					 }			 
				   } else{
                     conn->http_status=400;
				     conn->keep_alive=0;
				     minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										  "http protocol version head format is incorrect!");
					 return -1;	
				 }
	             index++;
				 offset= index+1;
				 end_of_line=1;
			 } else {
				   /* do nothing if it is not \r\n  */
				 
			 }
			 
			 break;
		  }
		  default:{
			  /*just skip the another @buf_c */
			  break;
		  }
	   }		
	}

	/* start to check if the http request url is legal */
	if(buffer_is_empty(conn->connection_request.request_url)){
		conn->http_status=400;
		conn->keep_alive=0;
        minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR, __FILE__,__LINE__,__FUNCTION__,
										  "http request url is empty!");
		return -1;
	}

	/*
	   start to parse http request option:
	   currently,we only support keep_alive/hosts/ranges options  
	*/
	uint32_t is_key=1;
	char * option_key=NULL,  *option_buf=NULL;
    for(;index<http_reuqest_len;index++) {
		char buf_c= request_content->ptr[index];
		if(is_key) {
            switch(buf_c){
			   case ':':
				   option_buf= (char*)request_content->ptr+(index+1);
				  is_key=0;
    		   default:
					break;				
			}  			
		} else{
			switch(buf_c) {
		  	    case '\r':{   
					if(request_content->ptr[index+1]=='\n'){    //it is end of an http request option
						request_content->ptr[index]='\0';
						request_content->ptr[index+1]='\0';

						option_key= (char*)request_content->ptr + offset;
						uint32_t option_buf_len= (char*)request_content->ptr + index - option_buf;
						if(option_buf_len>0) {
							  /* check if it connection option */
							if(strncasecmp(option_key,"Connection",sizeof("Connection"-1))==0){
								if(strncasecmp(option_buf,"keep-alive",sizeof("keep-alive")-1)==0){
								   conn->connection_request.keep_alive=HTTP_CONNECTION_KEEP_ALIVE ;				
								}
								else if(strncasecmp(option_buf,"close",sizeof("close")-1)==0){
									conn->connection_request.keep_alive=HTTP_CONNECTION_CLOSE;				   
								}
								else { /* ignore it */

								}
							}else if(strncasecmp(option_key,"Host",sizeof("Host")-1)==0){
								/* the hostname of client    */
								buffer_copy_string(conn->connection_request.hostname, option_buf);				 
							}else if(strncasecmp(option_key,"Range",sizeof("Range")-1)==0){
                                /*
								    client only request part of data from the url specified file,
									currently,only one part ranges is supported.
								 */
								buffer_copy_string(conn->connection_request.http_range, option_buf);
							}else{
								    /* for the rest options, we do not support now. just ignore it  */
								
							}					
						}else{
                                /*just ignore the option if the option value is empty */						
						}

						index++;
						offset = index +1;
						is_key=1;
						
					 } else{
						     /* an \n should follow \r in optiont value field */
						    conn->http_status=400;
						    conn->keep_alive=0;
                            minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR,
										   __FILE__,__LINE__,__FUNCTION__,"invalid http request option!");
							return -1;
				     }
					
				   break;	
				}
					
			    case  ' ':
			    case  '\t': {
					if(option_buf == (char*)request_content->ptr + index )
						option_buf= (char*)request_content->ptr+index+1;
			    }
			    default: break;
		     }		
 	    }
   }

     /* do some post-processsing to check if request parameter is leagal */
	if(conn->connection_request.http_version==HTTP_VERSION_1_1){
		if(conn->connection_request.keep_alive!=HTTP_CONNECTION_CLOSE)
			conn->keep_alive=1;
		else
			conn->keep_alive=0;
		
		if(buffer_is_empty(conn->connection_request.hostname)){
			    conn->http_status=400;
				conn->keep_alive=0;
                minihttpd_running_log(conn->p_worker->log_fd,MINIHTTPD_LOG_LEVEL_ERROR,
									  __FILE__,__LINE__,__FUNCTION__,"http request hostname is"  \
									   " missing with http1.1 version!");
			   return -1;            
		}
	}else{
		if(conn->connection_request.keep_alive==HTTP_CONNECTION_KEEP_ALIVE)
			conn->keep_alive=1;
		else conn->keep_alive=0;				
	}

	/*http reuqest head parse is done */
	return 0;
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


/*log parsed request to log file  */
void log_parsed_request(struct _connection * conn,request *r)
{

	assert(r!=NULL);
	buffer * b= buffer_init();
	
	buffer_copy_string(b,"\nhttp request head is parsed,the request detail is as following:\n");
	/*log http request method */
	buffer_append_string(b,"http_method:");
	if(!buffer_is_empty(r->http_method))
		buffer_append_string_length(b, (const char*)r->http_method->ptr,r->http_method->used_bytes-1);
	else buffer_append_string(b,"unset");
	buffer_append_string(b,"\n");

	/*log http request  */
	buffer_append_string(b,"http_url:");
	if(!buffer_is_empty(r->request_url))
		buffer_append_string_length(b, (const char*)r->request_url->ptr,r->request_url->used_bytes-1);
	else buffer_append_string(b,"unset");
	buffer_append_string(b,"\n");
	
	
	/*log http request protocol verison  */
	buffer_append_string(b,"http_protocol version:");
	if(r->http_version==HTTP_VERSION_1_1)
		buffer_append_string(b,"HTTP/1.1");
	else if(r->http_version==HTTP_VERSION_1_0)
		buffer_append_string(b,"HTTP/1.0");
	else buffer_append_string(b,"unset");
	buffer_append_string(b,"\n");


	/*log http request keep-alive flags  */
	buffer_append_string(b,"http_request keep_alive flags=");
	if(r->keep_alive==	  HTTP_CONNECTION_KEEP_ALIVE)
		buffer_append_string(b,"1");
	else if(r->keep_alive==	HTTP_CONNECTION_CLOSE)
       buffer_append_string(b,"=0");
	else buffer_append_string(b,"unset");
    buffer_append_string(b,"\n");
	

	/*log http request hostname */
	buffer_append_string(b,"http_request from host:");
	if(!buffer_is_empty(r->hostname))
	   buffer_append_string_length(b,(const char*)r->hostname->ptr,r->hostname->used_bytes-1);
	else buffer_append_string(b,"unset");
	buffer_append_string(b,"\n");
	   

	/* log http request ranges  */
	buffer_append_string(b,"http_request ranges:");
	if(!buffer_is_empty(r->http_range))
		buffer_append_string_length(b,(const char*)r->http_range->ptr,r->http_range->used_bytes-1);
	else buffer_append_string(b,"unset");
    buffer_append_string(b,"\n");

    minihttpd_running_log(conn->p_worker->log_fd, MINIHTTPD_LOG_LEVEL_INFO,
							 __FILE__,__LINE__,__FUNCTION__,"%s",(const char*)b->ptr);
    buffer_free(b);
}
