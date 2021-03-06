/*
    struct chunk: element contain buffer or target file which is going to be sent  

 */

#ifndef __CHUNK_H__
#define __CHUNK_H__

#include "buffer.h"

enum chunk_support_type {
	  CHUNK_MEM,
	  CHUNK_FILE,
};

typedef struct _chunk {
	buffer * mem;
    struct{
		int file_fd;   //file descriptor
		uint32_t offset;
		uint32_t length;
		buffer * filename;

		struct{
			void * mmap_start;    //start addr after  mmap 
			uint32_t mmap_offset; //the offset of file when call mmap 
			uint32_t mmap_len;	  // the length of file that has been mmaped 
		}mmap;
		
	}send_file;

	chunk_support_type chunk_type;
  
	/* the offset bytes that has been sent, the filed only can be used when in writequeue */
	uint32_t chunk_offset;

	/* chunk list  */
	struct _chunk*next;	
}chunk;

typedef struct{

	chunk * first;
	chunk* last;

	chunk *idle;
	uint32_t idle_chunk_size;
	
}chunkqueue;

/*initialize a chunkqueue */
chunkqueue* chunkqueue_init();

/*reset and free chunkqueue */
void chunkqueue_reset(chunkqueue*queue);
void chunkqueue_free(chunkqueue* queue);

/*append raw mem to chunkqueue*/
void chunkqueue_append_mem(chunkqueue*queue,void*ptr,uint32_t length);

/*append a buffer to chunkqueue */
void chunkqueue_append_buffer(chunkqueue*queue,buffer*mem);

/*prepend buffer to chunkqueue*/
void chunkqueue_prepend_buffer(chunkqueue*queue,buffer*mem);			 

/* append chunk to chunkqueue */
void chunkqueue_append_chunk(chunkqueue * queue, chunk* c);

/* append file data to chunkqueue */
void chunkqueue_append_file(chunkqueue * queue,buffer* filename,uint32_t offset, uint32_t len);

/*check if chunkqueue is empty*/
uint32_t chunkqueue_empty(chunkqueue*queue);

/*chunkqueue used buffer size */
uint32_t chunkqueue_length(chunkqueue * queue);

/*pick part of memory from chunkqueue */
void chunk_get_memory(chunkqueue * queue,  uint8_t** ptr, uint32_t * ptr_length);

/* commit the memory we have used  */
void chunk_commit_memory(chunkqueue * queue, uint32_t length);


/* mark some buffer is already sent while writing data to socket  */
uint32_t chunkqueue_mark_written(chunkqueue* queue,uint32_t len);


/* remove chunk which is already sent out  from writequeue and return the chunk  to idle list   */
void chunkqueue_remove_finished_chunk(chunkqueue * queue);


/* push unsed chunk to chunkqueue idle list  */
void chunkqueue_push_unused_chunk(chunkqueue* queue, chunk *c);

#endif 
