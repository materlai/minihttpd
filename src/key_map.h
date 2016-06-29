/*
     key-map:
	      key: the Identifier
		  map: the mapped target string 

 */


#ifndef __KEY_MAPH__
#define __KEY_MAPH__

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "types.h"


typedef struct{
	uint32_t key;
	const char *map_string;
}key_map;


typedef struct {
    char * extension;
	char * mime_type;	
}mime_type_entry;

typedef struct {
    mime_type_entry ** entry;
	/* mime entry count that has been filled in */
    uint32_t used_entry;
	/* total mime entry sizes  */
	uint32_t entry_sizes;	
}mime_table;


/*query http status string  according to http status code */
key_map *  query_http_status_string(uint32_t http_status_code);

/* initialize mime type tables from mime configuraiton file  */
mime_table* mime_table_initialize(const char * filename);

/* free mime_table */
void mime_table_free(mime_table*table);



#endif 
