#include "xmpp_socket.h"
#include "cbuffer.h"

#define KB *1024
#define MB *1024 KB

struct XMPPSOCKET_ITEM(socket_t)
{
   tinsock_socket_t sock;
   xmpp_conn_t * xmppconn;
   xmpp_ctx_t * xmppctx;
   settings_t settings;

   cbuffer_t swr_queue;
   cbuffer_t srd_queue;
   char* swr_buf;
   char* srd_buf;
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
   settings->rd_queue_size = settings->wr_queue_size = 1 MB;
}

static _init_socket(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   memset(settings, 0, sizeof(XMPPSOCKET_ITEM(socket_t)));
   xsock->sock = TS_SOCKET_ERROR;
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

static void _clear_queues(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   xsock->srd_queue->size = 0;
   xsock->srd_queue->offset = 0;
   xsock->swr_queue->size = 0;
   xsock->swr_queue->offset = 0;
}

static int _init_queues(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   xsock->srd_buf = xmpp_alloc(xsock->xmppctx, xsock->settings.rd_queue_size);
   if(!xsock->srd_buf)
      goto abort;
   xsock->swr_buf = xmpp_alloc(xsock->xmppctx, xsock->settings.wr_queue_size);
   if(!xsock->swr_buf)
      goto error1;
   xsock->srd_queue->capacity = xsock->settings.rd_queue_size;
   xsock->swr_queue->capacity = xsock->settings.wr_queue_size;
   _clear_queues(xsock);

   return 0;
error1:
   xmpp_free(xsock->xmppctx, xsock->srd_buf);
abort:
   return -1;
}

static void _deinit_queues(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   xsock->srd_queue->capacity = 0;
   xsock->swr_queue->capacity = 0;
   _clear_queues(xsock);

   if(xsock->srd_buf)
      xmpp_free(xsock->xmppctx, xsock->srd_buf);
   if(xsock->swr_buf)
      xmpp_free(xsock->xmppctx, xsock->swr_buf);
}

static int _update_queues(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   if(xsock->srd_queue->capacity == xsock->settings.rd_queue_size
      && xsock->swr_queue->capacity == xsock->settings.wr_queue_size)
      return;
   _deinit_queues(xsock);
   return _init_queues(xsock);
}

XMPPSOCKET_FUNCTION(int, connect_sock)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   assert(xsock->sock == TS_SOCKET_ERROR);
   if(_update_queues(xsock) == -1)
      goto abort;
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
   assert(xsock->sock == TS_SOCKET_ERROR);
   if(_update_queues(xsock) == -1)
      return ;
   if(xsock->sock == sock)
      return XS_OK;
   xsock->sock = sock;
   return XS_OK;
}

XMPPSOCKET_FUNCTION(int, run_once)(XMPPSOCKET_ITEM(socket_t) * xsock, unsigned long timeout)
{
   xmpp_run_once_foreign(xsock->xmppctx, timeout, &xsock->sock, 1, &xsock->sock, 1);

}
