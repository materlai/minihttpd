/*
     request.cpp 

 */

#include "request.h"

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
