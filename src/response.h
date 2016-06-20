/*
     response:  http response status  after handle http request   

 */

#ifndef __RESPONSE_H__
#define __RESPONSE_H__

#include "types.h"
#include "buffer.h"

typedef struct {

	/*keep-alive is one field of connection */

	/*content-length field */
	uint32_t content_length;

	/*HTTP_TRANSFER_ENCODING_CHUNKED is not used now  */

	/*content-type field*/
	buffer * content_type;	
}response;

#endif 
