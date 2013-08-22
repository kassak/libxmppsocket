#ifndef CBUFFER_H
#define CBUFFER_H

struct cbuffer_t
{
   int capacity;
   int size;
   int offset;
};

void* cbuffer_seq_avail_write(const cbuffer_t * cbuf, const void * buf, int * sz)
{
   if(cbuf->size + cbuf->offset <= cbuf->capacity)
      *sz = cbuf->capacity - cbuf->size - cbuf->offset;
   else
      *sz = cbuf->capacity - cbuf->size;
   return buf + cbuf->offset;
}

void* cbuffer_seq_avail_read(const cbuffer_t * cbuf, const void * buf, int * sz)
{
   if(cbuf->size + cbuf->offset <= cbuf->capacity)
      *sz = cbuf->size;
   else
      *sz = cbuf->capacity - cbuf->offset;
   return buf + cbuf->offset;
}

void cbuffer_read(const cbuffer_t * cbuf, int num)
{
   assert(cbuf->size >= num);
   cbuf->offset += num;
   if(cbuf->offset >= cbuf->capacity)
      cbuf->offset -= cbuf->capacity;
   cbuf->size -= num;
}

void cbuffer_write(const cbuffer_t * cbuf, int num)
{
   assert(cbuf->size + num <= cbuf->capacity);
   cbuf->size += num;
}

#endif //CBUFFER_H
