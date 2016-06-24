/*
     key-map:
	      key: the Identifier
		  map: the mapped target string 

 */

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


/* initialize mime type tables from mime configuraiton file  */
mime_table* mime_table_initialize(const char * filename);

/*query mimetype  from configuration mimetype tables */
int query_mimetype(mime_table * table,const char *extension,char *mime_type_name,uint32_t *length);


