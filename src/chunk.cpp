/*
     chunk.cpp 
 */

#include "chunk.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cassert>




/* initialize  chunk   */
void chunk_initialize(chunk *c)
{
	assert(c!=NULL);
	c->chunk_type=CHUNK_MEM;
	c->mem=buffer_init();
	c->send_file.file_fd=-1;
	c->send_file.filename=buffer_init();
	c->send_file.length=c->send_file.length=-1;
	c->next=NULL;
	c->chunk_offset=0;
}


/* reset a chunk, it type is CHUNK_TYPE by default   */
void chunk_reset(chunk *c)
{
	assert(c!=NULL);
	c->chunk_type=CHUNK_FILE;
	buffer_reset(c->mem);
	c->chunk_offset=0;
	c->next=NULL;

	buffer_reset(c->send_file.filename);
	if(c->send_file.file_fd>=0)
		close(c->send_file.file_fd);
	c->send_file.file_fd=-1;
    c->send_file.length=c->send_file.offset=0;
		
}

/*initialize a chunkqueue */
chunkqueue* chunkqueue_init()
{
	chunkqueue * queue= (chunkqueue*)malloc(sizeof(*queue));
	if(queue){
		queue->first=queue->last=queue->idle=NULL;
		queue->idle_chunk_size=0;
	}
	return queue;	
}

/*reset and free chunkqueue */
void chunkqueue_reset(chunkqueue*queue)
{
	if(!queue) return ;

	//free already used chunk 
	chunk*c = queue->first;
	for(;c;c=queue->first){
	    chunk*next=c->next; 	
	    buffer_free(c->mem);
		buffer_free(c->send_file.filename); 
		free((void*)c);
	    queue->first=next;	
	}
	//free unsed chunk 
	c=queue->idle;
	for(;c;c=queue->idle){
        chunk*next=c->next;
		buffer_free(c->mem);
		buffer_free(c->send_file.filename);
		free((void*)c);
		queue->idle=next;		
	}

	queue->first=queue->last=queue->idle=NULL;
	queue->idle_chunk_size=0;
}

void chunkqueue_free(chunkqueue* queue)
{
	if(!queue)  return ;
	//free already used chunk 
	chunk*c = queue->first;
	for(;c;c=queue->first){
	    chunk*next=c->next; 	
	    if(c->chunk_type==CHUNK_MEM && c->mem)
		  buffer_free(c->mem);
		free((void*)c);
	    queue->first=next;	
	}
	//free unsed chunk 
	c=queue->idle;
	for(;c;c=queue->idle){
        chunk*next=c->next;
		if(c->chunk_type==CHUNK_MEM && c->mem)
			buffer_free(c->mem);
		free((void*)c);
		queue->idle=next;		
	}
	free((void*)queue);
}


/*get unused chunk from chunkqueue */
chunk * chunkqueue_get_unused_chunk(chunkqueue* queue)
{
    assert(queue!=NULL);
	if(queue->idle_chunk_size==0){
		assert(queue->idle==NULL);
		queue->idle_chunk_size+=4;
		for(int index=0;index<queue->idle_chunk_size;index++){
            chunk *c = (chunk*) malloc(sizeof(chunk));
			chunk_initialize(c);
			
			c->next=queue->idle;
			queue->idle=c;						
		}
	}
	assert(queue->idle!=NULL);
	chunk * c= queue->idle;
	queue->idle=c->next;
	return c;
}


/* push unsed chunk to chunkqueue idle list  */

void chunkqueue_push_unused_chunk(chunkqueue* queue, chunk *c)
{
	assert(queue!=NULL && c!=NULL);
	if(queue->idle_chunk_size>=4){
		if(c->mem)  buffer_free(c->mem);
		if(c->send_file.filename)  buffer_free(c->send_file.filename);
		if(c->chunk_type==CHUNK_FILE && c->send_file.file_fd>=0){
            close(c->send_file.file_fd);
			c->send_file.file_fd=-1;			
		}
		free( (void*)c);
	}else{
		chunk_reset(c);
		c->next= queue->idle;
		queue->idle=c;
		queue->idle_chunk_size++;		
	}
}

/*append a buffer to chunkqueue */
void chunkqueue_append_buffer(chunkqueue*queue,buffer*mem)
{
  	if(!mem || ! queue)  return ;
	chunk * c=chunkqueue_get_unused_chunk(queue);
	c->chunk_type=CHUNK_MEM;
	buffer_copy_buffer(c->mem,mem);

	chunkqueue_append_chunk(queue,c);
	
}

/*prepend buffer to chunkqueue*/
void chunkqueue_prepend_buffer(chunkqueue*queue,buffer*mem)
{
	if(!mem || ! queue)  return ;
	chunk * c=chunkqueue_get_unused_chunk(queue);
	c->chunk_type=CHUNK_MEM;
	buffer_copy_buffer(c->mem,mem);

	c->next=queue->first;
	queue->first=c;

	if(queue->last==NULL) queue->last=c;
}

/*check if chunkqueue is empty*/
uint32_t  chunkqueue_empty(chunkqueue*queue)
{
	return !queue || queue->first==NULL;
	
}


/*chunkqueue used buffer size */
uint32_t  chunkqueue_length(chunkqueue * queue)
{
	  assert(queue!=NULL);
	  uint32_t len=0;
	  chunk * c=queue->first;
	  for(;c!=NULL;c=c->next){
		  if(c->chunk_type==CHUNK_MEM)  len+= buffer_string_length(c->mem);
		  else len+=c->send_file.length;		  
	  }
	  return len;
}

/* append chunk to chunkqueue */
void chunkqueue_append_chunk(chunkqueue * queue, chunk* c)
{
    assert(queue!=NULL && c!=NULL);
	c->next=NULL;
	if(queue->last)
        queue->last->next=c;
	queue->last=c;

	if(queue->first==NULL)  queue->first=c;
	
}



/*pick part of memory from chunkqueue */
void chunk_get_memory(chunkqueue * queue,  uint8_t** ptr, uint32_t * ptr_length)
{
    assert(queue!=NULL);
	assert( ptr!=NULL && ptr_length!=NULL);
	if(*ptr_length==0) {
		*ptr=NULL;
		return ;
	}
	
	chunk * c=queue->last;
	if(c && c->chunk_type==CHUNK_MEM){
		buffer * b= c->mem;
		assert(b!=NULL);
		uint32_t space= buffer_remaining_bytes(b);
		if(buffer_is_empty(b)){
            buffer_alloc_size(b,*ptr_length);
			space = buffer_remaining_bytes(b);
		}
		/* we have enough memory in current chunk */
		if(space>= *ptr_length){
            *ptr= b->ptr + buffer_string_length(b);
			return ;
		}		
	}

	// we need realloc a chunk
	c= chunkqueue_get_unused_chunk(queue);
    assert(c!=NULL);
	chunkqueue_append_chunk(queue,c);
	c->chunk_type=CHUNK_MEM;
    buffer * b= c->mem;
	buffer_alloc_size(b,*ptr_length);

	*ptr = b->ptr + buffer_string_length(b);
		
}

/* commit the memory we have used  */
void chunk_commit_memory(chunkqueue * queue, uint32_t length)
{
	assert(queue !=NULL && length>=0);
	if(length==0) return ;
	chunk * c=queue->last;
    assert(c!=NULL && c->chunk_type==CHUNK_MEM);
	if(buffer_string_length(c->mem)==0){
		c->mem->used_bytes = length +1;
		c->mem->ptr[c->mem->used_bytes-1]='\0';
	}else{
		c->mem->used_bytes+=length;
		c->mem->ptr[c->mem->used_bytes-1]='\0';
	}
	
}



/* append file data to chunkqueue */
void chunkqueue_append_file(chunkqueue * queue,buffer* filename,uint32_t offset, uint32_t len)
{
	assert(queue!=NULL && filename!=NULL);
	if(strlen((const char*)filename->ptr)<=0 ||  len<=0)  return ;

	chunk * c= chunkqueue_get_unused_chunk(queue);
	c->chunk_type=CHUNK_FILE;
    buffer_copy_buffer( c->send_file.filename,filename );
	c->send_file.file_fd=-1;
	c->send_file.offset=offset;
	c->send_file.length=len;

	/*append the chunk to used chunk list */
	chunkqueue_append_chunk(queue,c);
	return ;	
}


/* chunk remaining length   */
uint32_t chunk_remaining_len(chunk *c)
{
	 assert(c!=NULL);
	 if(c->chunk_type==CHUNK_MEM)   return buffer_string_length(c->mem)- c->chunk_offset;
	 else  return c->send_file.length-c->chunk_offset;
}

/* mark some buffer is already sent while writing data to socket  */
void chunkqueue_mark_written(chunkqueue* queue,uint32_t len)
{
    assert(queue!=NULL);
	chunk * c=queue->first;

	for(; c!=NULL;c=queue->first){
		uint32_t chunk_len= chunk_remaining_len(c);
		if(len==0 && chunk_len>0)  break;
		if(len>=chunk_len){
			c->chunk_offset+=chunk_len;
			len-=chunk_len;
			
			queue->first=c->next;
			if(queue->last==c)  queue->last=NULL;
			chunkqueue_push_unused_chunk(queue,c);			
		}else{

			c->chunk_offset+=len;
			len=0;
			break;			  			
		}		
	}
	assert(len==0);
}


/* remove chunk which is already sent out  from writequeue and return the chunk  to idle list   */
void chunkqueue_remove_finished_chunk(chunkqueue * queue)
{
	assert(queue!=NULL);

	chunk * c =queue->first;
	for(; c!=NULL  ; c=queue->first){
	    uint32_t len=chunk_remaining_len(c);
        if(len>0)  break;
		queue->first=c->next;
		if(queue->last==c)  queue->last=NULL;
		chunkqueue_push_unused_chunk(queue, c);		
	}
	
}
