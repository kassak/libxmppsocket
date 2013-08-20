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

enum {
   XS_ERROR,
   XS_OK,
}

struct XMPPSOCKET_ITEM(settings_t)
{
   const char * jid;
   const char * pass;
   const char * altdomain;
   const char * pair_jid;
   unsigned short altport;

   tinsock_sockaddr_storage_t addr;
};


struct XMPPSOCKET_ITEM(socket_t);

XMPPSOCKET_FUNCTION(int, init)();
XMPPSOCKET_FUNCTION(int, deinit)();
XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(socket_t) *, create)(xmpp_mem_t * allocator);
XMPPSOCKET_FUNCTION(void, dispose)(XMPPSOCKET_ITEM(socket_t) * xsock);
XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(settings_t) *, settings)(XMPPSOCKET_ITEM(socket_t) * xsock);
XMPPSOCKET_FUNCTION(int, connect_xmpp)(XMPPSOCKET_ITEM(socket_t) * xsock);
XMPPSOCKET_FUNCTION(int, connect_sock)(XMPPSOCKET_ITEM(socket_t) * xsock);
XMPPSOCKET_FUNCTION(int, pair_socket)(XMPPSOCKET_ITEM(socket_t) * xsock, tinsock_socket_t sock)
XMPPSOCKET_FUNCTION(int, run_once)(XMPPSOCKET_ITEM(socket_t) * xsock)

#undef XMPPSOCKET_PREFIX
#undef XMPPSOCKET_CONCAT2
#undef XMPPSOCKET_CONCAT
#undef XMPPSOCKET_ITEM
#undef XMPPSOCKET_FUNCTION

#endif // XMPP_SOCKET_H
