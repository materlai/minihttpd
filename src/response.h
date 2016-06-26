/*
     response:  http response status  after handle http request   

 */

#ifndef __RESPONSE_H__
#define __RESPONSE_H__

#include "types.h"
#include "buffer.h"
#include <sys/stat.h>

typedef struct {

	/*keep-alive is one field of connection */

	/* full path of the http request url */
	buffer * fullpath;

	/*  the stat struct of http request file or directory */
	struct stat s_stat;
	
	/*content-length field */
	uint32_t content_length;

	/*HTTP_TRANSFER_ENCODING_CHUNKED is not used now  */

	/*content-type field*/
	buffer * content_type;
	
}response;



/* initialize response   */
void response_initialize(response *r);

/* reset response  */
void response_reset(response *r);

/* fres response */
void response_free(response *r);


/*
    prepare http request response data:
	  1) check if http_reuqets url is legal
	  2) call stat(&filename,&st) to get file size to check if the file or directory which specified by request url  exists
	  3) if the file do not exist, check if we can safely handle parse the path,if we can not, return 404 
	  3) get the content-type from mine and store it 
	  4) call modules(mod_staticfile or mod_staticdir) to handle it 
 */
struct _connection ;
int http_request_handle(struct _connection * conn);



/* read directory content and return buffer which contain the directory entry*/
buffer * http_handle_directory_parse( struct _connection * conn,buffer*directory_path );


#endif 
