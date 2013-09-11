#ifndef XMPP_SOCKET_H
#define XMPP_SOCKET_H

#ifndef XMPPSOCKET_USER_PREFIX
#define XMPPSOCKET_PREFIX xmppsock_
#else
#define XMPPSOCKET_PREFIX XMPPSOCKET_USER_PREFIX
#endif

#define XMPPSOCKET_CONCAT2(X, Y) X##Y
#define XMPPSOCKET_CONCAT(X, Y) XMPPSOCKET_CONCAT2(X, Y)
#define XMPPSOCKET_ITEM(X)  XMPPSOCKET_CONCAT(XMPPSOCKET_PREFIX, X)
#define XMPPSOCKET_FUNCTION(RET, FOO) RET XMPPSOCKET_ITEM(FOO)

#include <libtinysocket/tinysocket.h>
#include <libstrophe/strophe.h>
#include <occam.h>

enum XMPPSOCKET_ITEM(result_t){
   XS_ERROR,
   XS_OK,
};

enum XMPPSOCKET_ITEM(errno_t){
   XS_EOK,
   XS_ETRANSMISSION,
   XS_EALLOCATION,
   XS_ELOGIC,
   XS_EFILTER,
};

typedef struct XMPPSOCKET_ITEM(filter_tag)
{
   void* (*init_state) (const void * in_data, int in_data_sz, size_t * out_sz);
   void (*deinit_state) (void * state);
   int (*filter)(const void * in_data, int in_data_sz,
          void * out_data, int out_data_sz,
          int * consumed, int * written, void * state);
   void (*last_error_str)(char * buf, int size);
} XMPPSOCKET_ITEM(filter_t);

typedef struct XMPPSOCKET_ITEM(settings_tag)
{
   const char * jid;
   const char * pass;
   const char * altdomain;
   const char * pair_jid;
   unsigned short altport;

   unsigned int rd_queue_size;
   unsigned int wr_queue_size;

   XMPPSOCKET_ITEM(filter_t) rd_filter;
   XMPPSOCKET_ITEM(filter_t) wr_filter;

   unsigned long latency;

   tinsock_sockaddr_storage_t addr;
} XMPPSOCKET_ITEM(settings_t);

typedef struct XMPPSOCKET_ITEM(errors_tag)
{
   int xs_errno;
   const char * xs_desc;
   int ts_errno;
} XMPPSOCKET_ITEM(errors_t);

struct XMPPSOCKET_ITEM(socket_tag);
typedef struct XMPPSOCKET_ITEM(socket_tag) XMPPSOCKET_ITEM(socket_t);

XMPPSOCKET_FUNCTION(int, init)();
XMPPSOCKET_FUNCTION(void, deinit)();
XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(socket_t) *, create)(const occam_allocator_t * allocator, occam_logger_t * log);
XMPPSOCKET_FUNCTION(void, dispose)(XMPPSOCKET_ITEM(socket_t) * xsock);
XMPPSOCKET_FUNCTION(int, connect_xmpp)(XMPPSOCKET_ITEM(socket_t) * xsock);
XMPPSOCKET_FUNCTION(int, connect_sock)(XMPPSOCKET_ITEM(socket_t) * xsock);
XMPPSOCKET_FUNCTION(int, pair_socket)(XMPPSOCKET_ITEM(socket_t) * xsock, tinsock_socket_t sock);
XMPPSOCKET_FUNCTION(int, run_once)(XMPPSOCKET_ITEM(socket_t) * xsock);

XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(settings_t) *, settings)(XMPPSOCKET_ITEM(socket_t) * xsock);
XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(errors_t) *, last_error)(XMPPSOCKET_ITEM(socket_t) * xsock);

XMPPSOCKET_FUNCTION(void, format_error)(XMPPSOCKET_ITEM(errors_t) * e, char * buf, int sz);

XMPPSOCKET_FUNCTION(const XMPPSOCKET_ITEM(filter_t)*, default_filter)();

#ifndef XMPPSOCKET_DO_NOT_UNDEFINE
   #undef XMPPSOCKET_PREFIX
   #undef XMPPSOCKET_CONCAT2
   #undef XMPPSOCKET_CONCAT
   #undef XMPPSOCKET_ITEM
   #undef XMPPSOCKET_FUNCTION
#endif

#endif // XMPP_SOCKET_H
