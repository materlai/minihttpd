/*
    custom minihttpd buffer 

 */

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "types.h"



typedef struct{
    uint8_t *ptr;        
	uint32_t used_bytes;  //used bytes for cueent buffer(include '\0')
	uint32_t size;   //the size which is already alloced for buffer
}buffer;

/*return initialize buffer*/
buffer* buffer_init(void);

/*free the memory space for buffer*/
void buffer_free(buffer*b);

/*reset buffer to default*/
void buffer_reset(buffer*b);

/*return bytes that has beed used for the buffer(not include '\0') */
uint32_t  buffer_string_length(buffer*b);

/*return remaining bytes for current buffer*/
uint32_t  buffer_remaining_bytes(buffer*b);


/*realloc buffer to contain sizes bytes*/
void buffer_alloc_size(buffer*b,uint32_t sizes);

/*copy a string to buffer(which is terminated by '\0')*/
void buffer_copy_string(buffer*b ,const char *src);

/*append string to buffer(parameter @string is terminated with '\0') */
void buffer_append_string(buffer*b,const char *string);

/*copy a buffer */
void buffer_copy_buffer(buffer*des,buffer*src);


/*check if  a buffer is empty*/
uint32_t buffer_is_empty(buffer*b);


#endif 
