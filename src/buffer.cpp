

#include "buffer.h"
#include <cassert>
/*return initialize buffer*/
buffer* buffer_init(void)
{
	buffer *b = (buffer*)malloc(sizeof(buffer));
	if(!b)   return NULL;
	b->ptr=NULL;
	b->size=0;
	b->used_bytes=0;
	return b;	
}

/*free the memory space for buffer*/
void buffer_free(buffer*b)
{
	if(b){
		if(b->ptr)  free((void*)b->ptr);
		free((void*)b);		
	}
}

/*reset buffer to default*/
void buffer_reset(buffer*b)
{
    if(b){
        if(b->ptr)   free((void*)b->ptr);
		b->ptr=NULL;
		b->used_bytes=0;
		b->size=0;		
	}
}

/*return bytes that has beed used for the buffer(not include '\0') */
uint32_t  buffer_string_length(buffer*b)
{
	assert(b!=NULL);
	return  (b && b->used_bytes>0)? b->used_bytes-1 :0;	
}

/*return remaining bytes for current buffer*/
uint32_t  buffer_remaining_bytes(buffer*b)
{
	assert(b!=NULL);
	if(b->used_bytes>0)  return b->size-b->used_bytes;
	return b->size;
}

uint32_t buffer_align_size(uint32_t sizes)
{
	const int fixed_chunk_size=64;
	return ( (sizes+fixed_chunk_size-1)/fixed_chunk_size)*fixed_chunk_size;
	
}

/*realloc buffer to contain sizes bytes*/
void buffer_alloc_size(buffer*b,uint32_t sizes)
{
	assert(b!=NULL);
    if(sizes==0)  sizes=1;
    if(b->size >= sizes)   return ;
	uint32_t align_sizes= buffer_align_size(sizes);
	if(b->ptr)  free((void*)b->ptr);
	b->ptr=(uint8_t*)malloc(align_sizes);
	b->used_bytes=0;
	b->size=align_sizes;	
}

/*copy a fixed length string to buffer*/
void buffer_copy_string_length(buffer*b,const char*src,uint32_t length)
{
    assert(b!=NULL && src!=NULL);
	buffer_alloc_size(b,length);
	memcpy(b->ptr,src,length);
	b->used_bytes=length;
	b->ptr[b->used_bytes-1]='\0';	   
}


/*copy a string to buffer(which is terminated by '\0')*/
void buffer_copy_string(buffer*b ,const char *src)
{
	buffer_copy_string_length(b,src,strlen(src)+1);
	
}


/*check if  a buffer is empty*/
uint32_t buffer_is_empty(buffer*b)
{
	return (b && b->used_bytes>0 )?0:1; 
	
}


/*reserve @sizes bytes memory space for buffer,if current buffer space is not enougn,realloc it */
void buffer_append_prepare_sizes(buffer*b,uint32_t sizes)
{
	assert(b && sizes>=0);
	if( buffer_remaining_bytes(b)<sizes) {
		uint32_t req_sizes= b->used_bytes + sizes;
		req_sizes=buffer_align_size(req_sizes);
		b->ptr= (uint8_t*)realloc(b->ptr,req_sizes);
		b->size=req_sizes; 
    }
}

/*append string to buffer(parameter @string is terminated with '\0') */
void buffer_append_string(buffer*b,const char *string)
{
     assert(b!=NULL && string!=NULL );
	 buffer_append_prepare_sizes(b,strlen(string)+1);
     if(b->used_bytes==0){
		 memcpy(b->ptr,string,strlen(string));
		 b->used_bytes=strlen(string)+1;
		 b->ptr[b->used_bytes-1]='\0';
	 }else{
		    assert(b->used_bytes>0);
	        memcpy(&b->ptr[b->used_bytes-1],string,strlen(string));
			b->used_bytes+=strlen(string);
			b->ptr[b->used_bytes-1]='\0';		 
	 }
}

/*copy a buffer */
void buffer_copy_buffer(buffer*des,buffer*src)
{
    assert(des!=NULL && src!=NULL);
	buffer_copy_string_length(des,(const char*)src->ptr,src->used_bytes);
	
}
