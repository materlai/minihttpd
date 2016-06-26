/*
    request : contain client http request 
 */

#ifndef __REQUEST_H__
#define __REQUEST_H__

#include "types.h"
#include "buffer.h"


typedef enum {
	HTTP_METHOD_UNSET = -1,
	HTTP_METHOD_GET,               /* [RFC2616], Section 9.3 */
	HTTP_METHOD_HEAD,              /* [RFC2616], Section 9.4 */
	HTTP_METHOD_POST,              /* [RFC2616], Section 9.5 */
	HTTP_METHOD_PUT,               /* [RFC2616], Section 9.6 */
	HTTP_METHOD_DELETE,            /* [RFC2616], Section 9.7 */
	HTTP_METHOD_CONNECT,           /* [RFC2616], Section 9.9 */
	HTTP_METHOD_OPTIONS,           /* [RFC2616], Section 9.2 */
	HTTP_METHOD_TRACE,             /* [RFC2616], Section 9.8 */
	HTTP_METHOD_ACL,               /* [RFC3744], Section 8.1 */
	HTTP_METHOD_BASELINE_CONTROL,  /* [RFC3253], Section 12.6 */
	HTTP_METHOD_BIND,              /* [RFC5842], Section 4 */
	HTTP_METHOD_CHECKIN,           /* [RFC3253], Section 4.4 and [RFC3253], Section 9.4 */
	HTTP_METHOD_CHECKOUT,          /* [RFC3253], Section 4.3 and [RFC3253], Section 8.8 */
	HTTP_METHOD_COPY,              /* [RFC4918], Section 9.8 */
	HTTP_METHOD_LABEL,             /* [RFC3253], Section 8.2 */
	HTTP_METHOD_LINK,              /* [RFC2068], Section 19.6.1.2 */
	HTTP_METHOD_LOCK,              /* [RFC4918], Section 9.10 */
	HTTP_METHOD_MERGE,             /* [RFC3253], Section 11.2 */
	HTTP_METHOD_MKACTIVITY,        /* [RFC3253], Section 13.5 */
	HTTP_METHOD_MKCALENDAR,        /* [RFC4791], Section 5.3.1 */
	HTTP_METHOD_MKCOL,             /* [RFC4918], Section 9.3 */
	HTTP_METHOD_MKREDIRECTREF,     /* [RFC4437], Section 6 */
	HTTP_METHOD_MKWORKSPACE,       /* [RFC3253], Section 6.3 */
	HTTP_METHOD_MOVE,              /* [RFC4918], Section 9.9 */
	HTTP_METHOD_ORDERPATCH,        /* [RFC3648], Section 7 */
	HTTP_METHOD_PATCH,             /* [RFC5789], Section 2 */
	HTTP_METHOD_PROPFIND,          /* [RFC4918], Section 9.1 */
	HTTP_METHOD_PROPPATCH,         /* [RFC4918], Section 9.2 */
	HTTP_METHOD_REBIND,            /* [RFC5842], Section 6 */
	HTTP_METHOD_REPORT,            /* [RFC3253], Section 3.6 */
	HTTP_METHOD_SEARCH,            /* [RFC5323], Section 2 */
	HTTP_METHOD_UNBIND,            /* [RFC5842], Section 5 */
	HTTP_METHOD_UNCHECKOUT,        /* [RFC3253], Section 4.5 */
	HTTP_METHOD_UNLINK,            /* [RFC2068], Section 19.6.1.3 */
	HTTP_METHOD_UNLOCK,            /* [RFC4918], Section 9.11 */
	HTTP_METHOD_UPDATE,            /* [RFC3253], Section 7.1 */
	HTTP_METHOD_UPDATEREDIRECTREF, /* [RFC4437], Section 7 */
	HTTP_METHOD_VERSION_CONTROL    /* [RFC3253], Section 3.5 */
} http_method_t;


typedef enum {
	            HTTP_VERSION_UNSET = -1,
			    HTTP_VERSION_1_0,
			    HTTP_VERSION_1_1
}http_version_t;


enum {
	  HTTP_CONNECTION_UNSET=0,
	  HTTP_CONNECTION_KEEP_ALIVE,
	  HTTP_CONNECTION_CLOSE
};


typedef struct {

   	/*  filed about request string from client */
	buffer *request_content;

	/*http method filed about http request*/
	buffer * http_method;
	/*url about http request*/
	buffer * request_url;
	/*http method about http request*/
	http_version_t http_version;

	/*
	     keep-alive option in http request head

	 */
	uint32_t  keep_alive;

	/* hostname of client */
	buffer * hostname;
    /*
      http request ranges field,
	  the field allow client to request  the server to sent part of file data back
	  currently, only one ranges is supported.
	*/
	buffer * http_range;
	
}request;


/*initialize a request */
void request_initialize(request * r);

/* free a request */
void request_free(request *r);

/*  request reset */
void request_reset(request *r);



/* get http request method from buffer @b  */
http_method_t http_get_request_method(buffer *b);

/*
    http request head parse :
	                         http_method,request_url,http_version
							 http_head: keep_alive,ranges,....
	

 */
struct _connection;
int http_reuqest_parse(struct _connection * conn);


/*log parsed request to log file  */
void log_parsed_request(struct _connection * conn,request *r);



#endif 
