#ifndef CBUFFER_H
#define CBUFFER_H

#include <assert.h>

typedef struct cbuffer_tag
{
   int capacity;
   int size;
   int offset;
} cbuffer_t;

static inline void* cbuffer_seq_avail_write(const cbuffer_t * cbuf, void * buf, int * sz)
{
   if(cbuf->size + cbuf->offset <= cbuf->capacity)
      *sz = cbuf->capacity - cbuf->size - cbuf->offset;
   else
      *sz = cbuf->capacity - cbuf->size;
   return buf + cbuf->offset;
}

static inline const void* cbuffer_seq_avail_read(const cbuffer_t * cbuf, const void * buf, int * sz)
{
   if(cbuf->size + cbuf->offset <= cbuf->capacity)
      *sz = cbuf->size;
   else
      *sz = cbuf->capacity - cbuf->offset;
   return buf + cbuf->offset;
}

static inline void cbuffer_read(cbuffer_t * cbuf, int num)
{
   //printf("read %i(%i)\n", num, cbuf->size);
   assert(cbuf->size >= num);
   cbuf->offset += num;
   if(cbuf->offset >= cbuf->capacity)
      cbuf->offset -= cbuf->capacity;
   cbuf->size -= num;
}

static inline void cbuffer_write(cbuffer_t * cbuf, int num)
{
   //printf("write %i(%i)\n", num, cbuf->size);
   assert(cbuf->size + num <= cbuf->capacity);
   cbuf->size += num;
   //printf("ns %i\n", cbuf->size);
}

#endif //CBUFFER_H
