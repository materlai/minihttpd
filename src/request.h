/*
    request : contain client http request 
 */

#ifndef __REQUEST_H__
#define __REQUEST_H__

#include "types.h"
#include "buffer.h"


typedef enum { HTTP_VERSION_UNSET = -1, HTTP_VERSION_1_0, HTTP_VERSION_1_1 } http_version_t;


typedef struct {

   	/*  filed about request string from client */
	buffer *request_content;

	/*http method filed about http request*/
	buffer * http_method;
	/*url about http request*/
	buffer * request_url;
	/*http method about http request*/
	http_version_t http_version;
	/* request ranges for http request */
	buffer * ranges;
	
}request;


/*initialize a request */
void request_initialize(request * r);

/* free a request */
void request_free(request *r);


#endif 
