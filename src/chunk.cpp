/*
     chunk.cpp 
 */

#include "chunk.h"
#include <cassert>

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
    if(!queue)  return NULL;
	if(queue->idle_chunk_size==0){
		assert(queue->idle==NULL);
		queue->idle_chunk_size+=4;
		for(int index=0;index<queue->idle_chunk_size;index++){
            chunk *c = (chunk*) malloc(sizeof(chunk));
			c->next=queue->idle;
			queue->idle=c;						
		}
	}
	assert(queue->idle!=NULL);
	chunk * c= queue->idle;
	queue->idle=c->next;
	return c;
}


/*append target file to chunkqueue*/
void chunkqueue_append_file(chunkqueue*queue,int file_fd,uint32_t offset,uint32_t length,buffer*filename)
{
    if(!queue)   return ;
	chunk * c= chunkqueue_get_unused_chunk(queue);
	c->chunk_type=CHUNK_FILE;
	c->send_file.file_fd=file_fd;
	c->send_file.filename=filename;
	c->send_file.length=length;
	c->send_file.offset=offset;

	if(queue->last){
		queue->last->next=c;
		queue->last=c;		
	}else{
        queue->last=c;		
	}

	if(queue->first==NULL)  queue->first=c;
}

/*append a buffer to chunkqueue */
void chunkqueue_append_buffer(chunkqueue*queue,buffer*mem)
{
  	if(!mem || ! queue)  return ;
	chunk * c=chunkqueue_get_unused_chunk(queue);
	c->chunk_type=CHUNK_MEM;
	c->mem=mem;
	c->next=NULL;

	if(queue->last){
		queue->last->next=c;
		queue->last=c;	 
	}
	else  queue->last=c;

	if(queue->first==NULL)  queue->first=c;
		
}

/*prepend buffer to chunkqueue*/
void chunkqueue_prepend_buffer(chunkqueue*queue,buffer*mem)
{
	if(!mem || ! queue)  return ;
	chunk * c=chunkqueue_get_unused_chunk(queue);
	c->chunk_type=CHUNK_MEM;
	c->mem=mem;
	c->next=NULL;

	c->next=queue->first;
	queue->first=c;
	if(queue->last==NULL)  queue->last=c;	
}

/*check if chunkqueue is empty*/
uint32_t  chunkqueue_empty(chunkqueue*queue)
{
	return !queue || queue->first==NULL;
	
}





