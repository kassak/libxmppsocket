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
   if(size + offset <= capacity)
      *sz = capacity - size - offset;
   else
      *sz = capacity - size;
   return buf + cbuf->offset;
}

void* cbuffer_seq_avail_read(const cbuffer_t * cbuf, const void * buf, int * sz)
{
   if(size + offset <= capacity)
      *sz = size;
   else
      *sz = capacity - offset;
   return buf + cbuf->offset;
}

#endif //CBUFFER_H
