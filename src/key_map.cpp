/*
     key_map.cpp

 */

#include "key_map.h"
#include <ctype.h>

#include <cassert>

static key_map  http_status_table[]= {
	{ 100, "Continue" },
	{ 101, "Switching Protocols" },
	{ 102, "Processing" }, /* WebDAV */
	{ 200, "OK" },
	{ 201, "Created" },
	{ 202, "Accepted" },
	{ 203, "Non-Authoritative Information" },
	{ 204, "No Content" },
	{ 205, "Reset Content" },
	{ 206, "Partial Content" },
	{ 207, "Multi-status" }, /* WebDAV */
	{ 300, "Multiple Choices" },
	{ 301, "Moved Permanently" },
	{ 302, "Found" },
	{ 303, "See Other" },
	{ 304, "Not Modified" },
	{ 305, "Use Proxy" },
	{ 306, "(Unused)" },
	{ 307, "Temporary Redirect" },
	{ 400, "Bad Request" },
	{ 401, "Unauthorized" },
	{ 402, "Payment Required" },
	{ 403, "Forbidden" },
	{ 404, "Not Found" },
	{ 405, "Method Not Allowed" },
	{ 406, "Not Acceptable" },
	{ 407, "Proxy Authentication Required" },
	{ 408, "Request Timeout" },
	{ 409, "Conflict" },
	{ 410, "Gone" },
	{ 411, "Length Required" },
	{ 412, "Precondition Failed" },
	{ 413, "Request Entity Too Large" },
	{ 414, "Request-URI Too Long" },
	{ 415, "Unsupported Media Type" },
	{ 416, "Requested Range Not Satisfiable" },
	{ 417, "Expectation Failed" },
	{ 422, "Unprocessable Entity" }, /* WebDAV */
	{ 423, "Locked" }, /* WebDAV */
	{ 424, "Failed Dependency" }, /* WebDAV */
	{ 426, "Upgrade Required" }, /* TLS */
	{ 500, "Internal Server Error" },
	{ 501, "Not Implemented" },
	{ 502, "Bad Gateway" },
	{ 503, "Service Not Available" },
	{ 504, "Gateway Timeout" },
	{ 505, "HTTP Version Not Supported" },
	{ 507, "Insufficient Storage" }, /* WebDAV */
	{ -1, NULL }
};




/*query http status string  according to http status code */
key_map *  query_http_status_string(uint32_t http_status_code)
{
	for(uint32_t index=0;http_status_table[index].map_string!=NULL;index++){
        if(http_status_table[index].key==http_status_code)
			return &http_status_table[index];
	}
	return NULL;
}



/*   initialize mime type tables from mime configuraiton file:
     return mime_table is not NULL for successfully
	 else  return NULL;

 */
mime_table* mime_table_initialize(const char * filename)
{

	assert( filename !=NULL );
	if(strlen(filename)<=0)   return NULL;

	mime_table * table= (mime_table*)malloc(sizeof(*table));
    assert(table!=NULL);
    table->entry_sizes=1024; //by default set to max 1024 entrys
	table->used_entry=0;
	table->entry= (mime_type_entry**) malloc(table->entry_sizes * sizeof(*table->entry));

	FILE * fp =fopen(filename,"r");
	if(!fp){
		free((void*)table->entry);
		free((void*)table);
		table=NULL;
		return table;		
	}
    char mime_buf[1024];
	while(fgets(mime_buf,sizeof(mime_buf),fp)!=NULL){
        /*we read a new line */
		if(strlen(mime_buf)<=0 ||  mime_buf[0]=='#')
			continue;

		/*
		     one line mime type format :   extension => mime-type 

		 */
		
		char * extension_end= NULL;
		char * mime_type_start=NULL;
		if(  (extension_end= strstr(mime_buf,"=>"))==NULL ||  extension_end==mime_buf)
			continue;

		mime_type_start=extension_end+sizeof("=>")-1;		

		//start to parse extension
		uint32_t extension_start_index=0;
		while(isspace(mime_buf[extension_start_index]))
			extension_start_index++;
		if(mime_buf[extension_start_index]!='"')  continue;
		extension_start_index++;

		uint32_t extension_end_index= extension_start_index;
		while(mime_buf[extension_end_index]!='"'  &&  (&mime_buf[extension_end_index]!=extension_end))
			extension_end_index++;

		if(&mime_buf[extension_end_index]==extension_end)  continue;

		/* we get the extension now  */
		uint32_t extension_len= extension_end_index- extension_start_index;
		if(extension_len==0)  continue;

		/*start to parse mime type */
		uint32_t mime_type_start_index= mime_type_start - mime_buf;
		while( isspace(mime_buf[mime_type_start_index])  )
			mime_type_start_index++;
		if(mime_buf[mime_type_start_index]!='"') continue;
		mime_type_start_index++;

		uint32_t mime_type_end_index=  mime_type_start_index;
		while(mime_buf[mime_type_end_index]!='\0' && mime_buf[mime_type_end_index]!='"' )
			mime_type_end_index++;

		if(mime_buf[mime_type_end_index]=='\0')   continue;

		/*we got the mime type string */
		uint32_t mime_type_len= mime_type_end_index-mime_type_start_index;
		if(mime_type_len==0)  continue;
		
		/*
            valid   extension -> mime-type map is already			
		 */
		if(table->used_entry==table->entry_sizes){
			/* we need to realloc table entry */
			table->entry_sizes+=1024;
			table->entry= (mime_type_entry**) realloc(table->entry,
													table->entry_sizes*sizeof(*table->entry))  ;
		}
		
		table->entry[table->used_entry]=(mime_type_entry*) malloc(sizeof(mime_type_entry));
        table->entry[table->used_entry]->extension=(char*) malloc(extension_len+1);
		memset(table->entry[table->used_entry]->extension,0,extension_len+1);
		memcpy(table->entry[table->used_entry]->extension, &mime_buf[extension_start_index],
			     extension_len);

		table->entry[table->used_entry]->mime_type= (char*) malloc(mime_type_len+1);
		memset(table->entry[table->used_entry]->mime_type,0,mime_type_len+1);
		memcpy(table->entry[table->used_entry]->mime_type,&mime_buf[mime_type_start_index],mime_type_len);

		table->used_entry++;
		
	}


	fclose(fp);
	return table;
}


/* free mime_table */
void mime_table_free(mime_table*table)
{
	assert(table!=NULL);
	for(uint32_t entry_index=0;entry_index<table->used_entry;entry_index++){
		mime_type_entry * entry=table->entry[entry_index];
		assert(entry!=NULL);
		//free entry
		if(entry->extension) free((void*)entry->extension);
		if(entry->mime_type) free((void*)entry->mime_type);
		free((void*)entry);
		table->entry[entry_index]=NULL;
	}
	
	//free the whole table entry 
	free( (void*) table->entry);
	//free the whole table
	free( (void*)table );
 }


