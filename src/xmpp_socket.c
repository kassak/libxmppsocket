#define XMPPSOCKET_DO_NOT_UNDEFINE
#include "xmpp_socket.h"
#include "cbuffer.h"
#include <libtinysocket/tinysocket.h>
#include <string.h>
#include <assert.h>

#define KB *1024
#define MB *1024 KB

#define PRECONDITION(X, S) if(!(X)) {_fill_error(S, XS_ELOGIC, "precondition failed: " #X);assert(0);return XS_ERROR;}

struct XMPPSOCKET_ITEM(socket_tag)
{
   tinsock_socket_t sock;
   xmpp_conn_t * xmppconn;
   xmpp_ctx_t * xmppctx;
   XMPPSOCKET_ITEM(settings_t) settings;
   XMPPSOCKET_ITEM(errors_t) last_error;

   cbuffer_t swr_queue;
   cbuffer_t srd_queue;
   char* swr_buf;
   char* srd_buf;
};

static void _clear_sock_error(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   memset(&xsock->last_error, 0, sizeof(XMPPSOCKET_ITEM(errors_t)));
}

static void _fill_error(XMPPSOCKET_ITEM(socket_t) * xsock, int xs_error, const char * desc)
{
   _clear_sock_error(xsock);
   xsock->last_error.xs_errno = xs_error;
   xsock->last_error.xs_desc = desc;
}

static void _fill_filter_error(XMPPSOCKET_ITEM(socket_t) * xsock, int xs_error, const char * desc, XMPPSOCKET_ITEM(filter_t) * flt, void * state)
{
   _clear_sock_error(xsock);
   xsock->last_error.xs_errno = xs_error;
   xsock->last_error.xs_desc = desc;
   //TODO: filter error
}

static void _fill_sock_error(XMPPSOCKET_ITEM(socket_t) * xsock, int xs_error, const char * desc)
{
   _fill_error(xsock, xs_error, desc);
   xsock->last_error.ts_errno = tinsock_last_error();
}

XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(errors_t) *, last_error)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   return &xsock->last_error;
}

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
   settings->run_timeout = 10;
   settings->rd_filter = *XMPPSOCKET_ITEM(default_filter)();
   settings->wr_filter = *XMPPSOCKET_ITEM(default_filter)();
}

static _init_socket(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   memset(xsock, 0, sizeof(XMPPSOCKET_ITEM(socket_t)));
   xsock->sock = TS_SOCKET_ERROR;
   _init_settings(&xsock->settings);
}

XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(socket_t) *, create)(xmpp_mem_t * allocator, int log_level)
{
   xmpp_log_t * log = NULL;
   if(log_level != -1)
      log = xmpp_get_default_logger(log_level);
   xmpp_ctx_t * ctx = xmpp_ctx_new(allocator, log);

   if(!ctx)
      goto abort;

   XMPPSOCKET_ITEM(socket_t) * xsock = xmpp_alloc(ctx, sizeof(XMPPSOCKET_ITEM(socket_t)));
   _init_socket(xsock);
   xsock->settings.xmpp_log_level = log_level;
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
      tinsock_close(xsock->sock);
   if(xsock->xmppconn)
      xmpp_conn_release(xsock->xmppconn);
   if(xsock->xmppctx)
      xmpp_ctx_free(xsock->xmppctx);
}

XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(settings_t) *, settings)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   return &xsock->settings;
}

static int _msg_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza,
                        void * const userdata)
{
   XMPPSOCKET_ITEM(socket_t) * xsock = (XMPPSOCKET_ITEM(socket_t) *)userdata;
   char *intext;

   if(!xmpp_stanza_get_child_by_name(stanza, "body"))
      return 1;
   if(!strcmp(xmpp_stanza_get_attribute(stanza, "type"), "error"))
      return 1;

   intext = xmpp_stanza_get_text(xmpp_stanza_get_child_by_name(stanza, "body"));

   int size = strlen(intext);
   if(size == 0)
      return 1;
   void * state = xsock->settings.wr_filter.init_state(intext, size);
   for(;;)
   {
      int sz, consumed, written;
      void * buf = cbuffer_seq_avail_write(&xsock->swr_queue, xsock->swr_buf, &sz);
      if(sz == 0) // pitty, not enough space in queue
      {
         _fill_error(xsock, XS_EFILTER, "write queue overflow");
         goto release;
      }
      if(xsock->settings.wr_filter.filter(intext, size, buf, sz, &consumed, &written, state))
      {
         _fill_filter_error(xsock, XS_EFILTER, "filter failed", &xsock->settings.wr_filter, state);
         goto release;
      }
      if(written > sz) // oups, filter gone crazy and wrote outside queu
      {
         _fill_error(xsock, XS_EFILTER, "filter wrote too much");
         goto release;
      }

      //update buffers
      cbuffer_write(&xsock->swr_queue, written);
      intext += consumed;
      size -= consumed;

      if(size < 0) // oups, filter gone crazy and ate outside string
      {
         _fill_error(xsock, XS_EFILTER, "filter consumed too much");
         goto release;
      }
      //okay, everything is now in queue
      if(size == 0)
         break;
   }
release:
   xsock->settings.wr_filter.deinit_state(state);
   return 1;
}

static void _conn_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t event,
     const int error, xmpp_stream_error_t * const stream_error, void * const userdata)
{
   XMPPSOCKET_ITEM(socket_t) * xsock = (XMPPSOCKET_ITEM(socket_t) *)userdata;
   if (event == XMPP_CONN_CONNECT)
   {
      printf("connected\n");
      xmpp_stanza_t* pres;
      //      xmpp_handler_add(conn,version_handler, "jabber:iq:version", "iq", NULL, xsock);
      xmpp_handler_add(conn, _msg_handler, NULL, "message", NULL, xsock);

      /* Send initial <presence/> so that we appear online to contacts */
      pres = xmpp_stanza_new(xsock->xmppctx);
      xmpp_stanza_set_name(pres, "presence");
      xmpp_send(conn, pres);
      xmpp_stanza_release(pres);
   }
   else
   {
      printf("disconnected %i\n", error);
   }
}

XMPPSOCKET_FUNCTION(int, connect_xmpp)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   xmpp_conn_set_jid(xsock->xmppconn, xsock->settings.jid);
   xmpp_conn_set_pass(xsock->xmppconn, xsock->settings.pass);

   /* initiate connection */
   if(xmpp_connect_client(xsock->xmppconn, xsock->settings.altdomain,
         xsock->settings.altport, _conn_handler, xsock))
   {
      _fill_sock_error(xsock, XS_ETRANSMISSION, "xmpp connection failed");
      goto abort;
   }

   return XS_OK;
abort:
   return XS_ERROR;
}

static void _clear_queues(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   xsock->srd_queue.size = 0;
   xsock->srd_queue.offset = 0;
   xsock->swr_queue.size = 0;
   xsock->swr_queue.offset = 0;
}

static int _init_queues(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   xsock->srd_buf = xmpp_alloc(xsock->xmppctx, xsock->settings.rd_queue_size);
   if(!xsock->srd_buf)
   {
      _fill_error(xsock, XS_EALLOCATION, "failed to allocate read buffer");
      goto abort;
   }
   xsock->swr_buf = xmpp_alloc(xsock->xmppctx, xsock->settings.wr_queue_size);
   if(!xsock->swr_buf)
   {
      _fill_error(xsock, XS_EALLOCATION, "failed to allocate write buffer");
      goto error1;
   }
   xsock->srd_queue.capacity = xsock->settings.rd_queue_size;
   xsock->swr_queue.capacity = xsock->settings.wr_queue_size;
   _clear_queues(xsock);

   return 0;
error1:
   xmpp_free(xsock->xmppctx, xsock->srd_buf);
abort:
   return -1;
}

static void _deinit_queues(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   xsock->srd_queue.capacity = 0;
   xsock->swr_queue.capacity = 0;
   _clear_queues(xsock);

   if(xsock->srd_buf)
      xmpp_free(xsock->xmppctx, xsock->srd_buf);
   if(xsock->swr_buf)
      xmpp_free(xsock->xmppctx, xsock->swr_buf);
}

static int _update_queues(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   if(xsock->srd_queue.capacity == xsock->settings.rd_queue_size
      && xsock->swr_queue.capacity == xsock->settings.wr_queue_size)
      return;
   _deinit_queues(xsock);
   return _init_queues(xsock);
}

static int _addr_size_by_family(int family)
{
   if(family == TS_AF_INET)
      return sizeof(tinsock_sockaddr_in_t);
   else if(family == TS_AF_INET)
      return sizeof(tinsock_sockaddr_in6_t);
   assert(!"unsupported family");
   return 0;
}

XMPPSOCKET_FUNCTION(int, connect_sock)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   PRECONDITION(xsock->sock == TS_SOCKET_ERROR, xsock);

   if(_update_queues(xsock) == -1)
      goto abort;
   xsock->sock = tinsock_socket(((const tinsock_sockaddr_t*)&xsock->settings.addr)->sa_family,
          TS_SOCK_STREAM, TS_IPPROTO_UNSPECIFIED);
   if(xsock->sock == TS_SOCKET_ERROR)
   {
      _fill_sock_error(xsock, XS_ETRANSMISSION, "failed to create socket");
      goto abort;
   }

   if(TS_NO_ERROR != tinsock_connect(xsock->sock, (const tinsock_sockaddr_t*)&xsock->settings.addr,
                                     _addr_size_by_family(xsock->settings.addr.ss_family)))
   {
      _fill_sock_error(xsock, XS_ETRANSMISSION, "failed to connect");
      goto error1;
   }
   return XS_OK;
error1:
   tinsock_close(xsock->sock);
abort:
   return XS_ERROR;
}

XMPPSOCKET_FUNCTION(int, pair_socket)(XMPPSOCKET_ITEM(socket_t) * xsock, tinsock_socket_t sock)
{
   PRECONDITION(xsock->sock == TS_SOCKET_ERROR, xsock);

   if(_update_queues(xsock) == -1)
      return XS_ERROR;
   xsock->sock = sock;
   return XS_OK;
}

XMPPSOCKET_FUNCTION(int, run_once)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   PRECONDITION(xsock->sock != TS_SOCKET_ERROR, xsock);
   PRECONDITION(xsock->xmppconn != NULL, xsock);

   xmpp_run_once_foreign(xsock->xmppctx, xsock->settings.run_timeout, &xsock->sock, 1, &xsock->sock, 1);
   //read from socket
   {
      int sz;
      void * buf;
      buf = cbuffer_seq_avail_write(&xsock->srd_queue, xsock->srd_buf, &sz);
      if(sz)
      {
         int res = tinsock_read(xsock->sock, buf, sz);
         if(res == -1 && !tinsock_is_recoverable())
         {
            _fill_sock_error(xsock, XS_ETRANSMISSION, "error reading from socket");
            return XS_ERROR;
         }
         else if(res == 0)
         {
            //TODO: socket closed?
         }
         else if(res != -1)
         {
            cbuffer_write(&xsock->srd_queue, res);
         }
      }
   }
   //write to socket
   {
      int sz;
      const void * buf;
      buf = cbuffer_seq_avail_read(&xsock->srd_queue, xsock->srd_buf, &sz);
      if(sz)
      {
         int res = tinsock_write(xsock->sock, buf, sz);
         if(res == -1 && !tinsock_is_recoverable())
         {
            _fill_sock_error(xsock, XS_ETRANSMISSION, "error writing to socket");
            return XS_ERROR;
         }
         else if(res == 0)
         {
            //TODO: socket closed?
         }
         else if(res != -1)
         {
            cbuffer_read(&xsock->srd_queue, res);
         }
      }
   }
   return XS_OK;
}

static void* _init_dummy_state(const void * data, int sz)
{
}

static void _deinit_dummy_state(void * state)
{
}

static int _transfer_filter(const void * in_data, int in_data_sz,
          void * out_data, int out_data_sz,
          int * consumed, int * written, void * state)
{
   int tr = (out_data_sz < in_data_sz) ? out_data_sz : in_data_sz;
   assert(tr > 0);
   *consumed = *written = tr;
   memcpy(out_data, in_data, tr);
   return 0;
}

static void _last_dummy_error_str(char * buf, int size)
{
   strncpy(buf, "no, that can't be =)", size);
   if(size > 0) //just to be sure
       buf[size - 1] = '\0';
}


XMPPSOCKET_FUNCTION(const XMPPSOCKET_ITEM(filter_t)*, default_filter)()
{
   static XMPPSOCKET_ITEM(filter_t) def_filter = {
      _init_dummy_state,
      _deinit_dummy_state,
      _transfer_filter,
      _last_dummy_error_str
   };
   return &def_filter;
}

XMPPSOCKET_FUNCTION(void, format_error)(XMPPSOCKET_ITEM(errors_t) * e, char * buf, int sz)
{
   if(e->xs_errno == XS_EOK)
   {
      buf[0] = '\0';
      return;
   }
   if(e->ts_errno == 0)
   {
      strncpy(buf, e->xs_desc, sz);
      if(sz != 0)
         buf[sz-1] = '\0';
   }
   snprintf(buf, sz, "%s: %s", e->xs_desc, strerror(e->ts_errno));
}
