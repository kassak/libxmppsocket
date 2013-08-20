#include "xmpp_socket.h"

struct XMPPSOCKET_ITEM(socket_t)
{
   tinsock_socket_t sock;
   xmpp_conn_t * xmppconn;
   xmpp_ctx_t * xmppctx;
   settings_t settings;
};


XMPPSOCKET_FUNCTION(int, init)()
{
   xmpp_initialize();
   tinsock_init();
}
XMPPSOCKET_FUNCTION(int, deinit)()
{
   tinsock_deinit();
   xmpp_shutdown();
}

static _init_settings(XMPPSOCKET_ITEM(settings_t) * settings)
{
   memset(settings, 0, sizeof(XMPPSOCKET_ITEM(settings_t)));
}

static _init_socket(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   xsock->sock = TS_SOCKET_ERROR;
   xsock->xmppcon = xsock->xmppctx = NULL;
   _init_settings(&xsock->settings);
}

XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(socket_t) *, create)(xmpp_mem_t * allocator)
{
   xmpp_ctx_t * ctx = xmpp_ctx_new(allocator, NULL); //   log = xmpp_get_default_logger(XMPP_LEVEL_DEBUG);

   if(!ctx)
      goto abort;

   XMPPSOCKET_ITEM(socket_t) * xsock = xmpp_alloc(ctx, sizeof(XMPPSOCKET_ITEM(socket_t)));
   _init_socket(xsock);
   if(!xsock)
      goto error1;

   xsock->xmppctx = ctx;
   xsock->xmppconn = xmpp_conn_new(ctx);
   if(!xsock->xmppconn)
      goto error2;

   return xsock;

//error handling
error2:
   xmpp_free(ctx, xsock);
error1:
   xmpp_ctx_free(ctx);
abort:
   return NULL;
}

XMPPSOCKET_FUNCTION(void, dispose)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   if(xsock->sock != TS_SOCKET_ERROR)
      tinsocket_close(xsock->sock);
   xmpp_conn_release(conn);
   xmpp_ctx_free(ctx);
}

XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(settings_t) *, settings)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   return &xsock->settings;
}

XMPPSOCKET_FUNCTION(int, connect_xmpp)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   xmpp_conn_set_jid(xsock->xmppconn, settings->jid);
   xmpp_conn_set_pass(xsock->xmppconn, settings->pass);

   /* initiate connection */
   if(xmpp_connect_client(xsock->xmppconn, settings->altdomain, settings->altport, conn_handler, xsock))
      goto abort;

   return XS_OK;
abort:
   return XS_ERROR;
}

XMPPSOCKET_FUNCTION(int, connect_sock)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   xsock->sock = tinsock_socket(settings.addr->__sa_family, TS_SOCK_STREAM, TS_IPPROTO_UNSPECIFIED);
   if(xsock->sock == TS_SOCKET_ERROR)
      goto abort;

   if(TS_NO_ERROR != tinsock_connect(xsock->sock, (const tinsock_sockaddr*)(settings->addr),
                                     sizeof(tinsock_sockaddr_storage_t)))
      goto error1;
   return XS_OK;
error1:
   tinsock_close(xsock->sock);
abort:
   return XS_ERROR;
}

XMPPSOCKET_FUNCTION(int, pair_socket)(XMPPSOCKET_ITEM(socket_t) * xsock, tinsock_socket_t sock)
{
   if(xsock->sock == sock)
      return XS_OK;
   if(xsock->sock != TS_SOCKET_ERROR)
   {
      tinsock_close(xsock->sock);
      xsock->sock = TS_SOCKET_ERROR;
   }
   xsock->sock = sock;
   return XS_OK;
}

XMPPSOCKET_FUNCTION(int, run_once)(XMPPSOCKET_ITEM(socket_t) * xsock, unsigned long timeout)
{
   xmpp_run_once_foreign(xsock->xmppctx, timeout, &xsock->sock, 1, &xsock->sock, 1);

}
