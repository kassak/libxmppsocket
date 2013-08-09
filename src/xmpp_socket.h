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

struct XMPPSOCKET_ITEM(socket_t)
{
   tinsock_socket_t sock;
   xmpp_conn_t * xmppconn;
};

#undef XMPPSOCKET_PREFIX
#undef XMPPSOCKET_CONCAT2
#undef XMPPSOCKET_CONCAT
#undef XMPPSOCKET_ITEM
#undef XMPPSOCKET_FUNCTION

#endif // XMPP_SOCKET_H