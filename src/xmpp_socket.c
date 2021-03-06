#define XMPPSOCKET_DO_NOT_UNDEFINE
#define OCCAM_STANDARD_ALLOCATOR
#include "xmpp_socket.h"
#include "cbuffer.h"
#include <libtinysocket/tinysocket.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define KB *1024
#define MB *1024 KB

#define PRECONDITION(X, S) if(!(X)) {_fill_error(S, XS_ELOGIC, "precondition failed: " #X);assert(0);return XS_ERROR;}
#define SOCKADDR(ADDR) ((tinsock_sockaddr_t*)(ADDR))
#define SOCKADDR_IP(ADDR) ((ADDR)->ss_family == TS_AF_INET ? (void*)&((tinsock_sockaddr_in_t*)(ADDR))->sin_addr : (void*)&((tinsock_sockaddr_in6_t*)(ADDR))->sin6_addr)
#define SOCKADDR_PORT(ADDR) tinsock_ntohs((ADDR)->ss_family == TS_AF_INET ? ((tinsock_sockaddr_in_t*)(ADDR))->sin_port : ((tinsock_sockaddr_in6_t*)(ADDR))->sin6_port)

struct XMPPSOCKET_ITEM(socket_tag)
{
   tinsock_socket_t sock;
   xmpp_conn_t * xmppconn;
   xmpp_ctx_t * xmppctx;
   XMPPSOCKET_ITEM(settings_t) settings;
   XMPPSOCKET_ITEM(errors_t) last_error;

   clock_t last_tm;

   cbuffer_t swr_queue;
   cbuffer_t srd_queue;
   char* swr_buf;
   char* srd_buf;

   const occam_logger_t * log;
   const occam_allocator_t * mem;
};

static clock_t _msecs_passed(XMPPSOCKET_ITEM(socket_t) * xsock, clock_t tm)
{
   clock_t dclock = xsock->last_tm - tm;
   return dclock * 1000 / CLOCKS_PER_SEC;
}

static void _clear_sock_error(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   memset(&xsock->last_error, 0, sizeof(XMPPSOCKET_ITEM(errors_t)));
}

static void _fill_error(XMPPSOCKET_ITEM(socket_t) * xsock, int xs_error, const char * desc)
{
   _clear_sock_error(xsock);
   xsock->last_error.xs_errno = xs_error;
   xsock->last_error.xs_desc = desc;
   occam_log_t(xsock->log, "filling error: %s", desc);
}

static void _fill_filter_error(XMPPSOCKET_ITEM(socket_t) * xsock, int xs_error, const char * desc, XMPPSOCKET_ITEM(filter_t) * flt, void * state)
{
   _fill_error(xsock, xs_error, desc);
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
   return tinsock_init();
}
XMPPSOCKET_FUNCTION(void, deinit)()
{
   tinsock_deinit();
   xmpp_shutdown();
}

static void _init_settings(XMPPSOCKET_ITEM(settings_t) * settings)
{
   memset(settings, 0, sizeof(XMPPSOCKET_ITEM(settings_t)));
   settings->rd_queue_size = settings->wr_queue_size = 1 MB;
   settings->latency = 500;
   settings->rd_filter = *XMPPSOCKET_ITEM(default_filter)();
   settings->wr_filter = *XMPPSOCKET_ITEM(default_filter)();
}

static void _init_socket(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   memset(xsock, 0, sizeof(XMPPSOCKET_ITEM(socket_t)));
   xsock->sock = TS_SOCKET_ERROR;
   xsock->last_tm = clock();
   _init_settings(&xsock->settings);
}

XMPPSOCKET_FUNCTION(XMPPSOCKET_ITEM(socket_t) *, create)(const occam_allocator_t * allocator, occam_logger_t * log)
{
   if(!allocator)
      allocator = &occam_standard_allocator;
   xmpp_ctx_t * ctx = xmpp_ctx_new(allocator, log);

   if(!ctx)
      goto abort;

   XMPPSOCKET_ITEM(socket_t) * xsock = occam_alloc(allocator, sizeof(XMPPSOCKET_ITEM(socket_t)));
   _init_socket(xsock);
   xsock->log = log;
   xsock->mem = allocator;
   if(!xsock)
      goto error1;

   xsock->xmppctx = ctx;
   xsock->xmppconn = xmpp_conn_new(ctx);
   if(!xsock->xmppconn)
      goto error2;

   return xsock;

//error handling
error2:
   occam_free(allocator, xsock);
error1:
   xmpp_ctx_free(ctx);
abort:
   occam_logs_d(log, "xmpp socket creation failed");
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
   assert(xsock->settings.pair_jid);

   if(!xmpp_stanza_get_child_by_name(stanza, "body"))
      return 1;
   if(!strcmp(xmpp_stanza_get_attribute(stanza, "type"), "error"))
      return 1;
   if(!strcmp(xmpp_stanza_get_attribute(stanza, "from"), xsock->settings.pair_jid)) //ignore others
   {
      occam_log_d(xsock->log, "recieved message from %s but assumed %s. who're you, you son of the bitch?",
          xmpp_stanza_get_attribute(stanza, "from"), xsock->settings.pair_jid);
      return 0;
   }

   char *intext;
   intext = xmpp_stanza_get_text(xmpp_stanza_get_child_by_name(stanza, "body"));

   int size = strlen(intext);
   if(size == 0)
      return 1;
   occam_log_d(xsock->log, "read %i bytes from xmpp", size);

   size_t out_sz;
   void * state = xsock->settings.wr_filter.init_state(intext, size, &out_sz);
   //occam_log_d(xsock->log, "out_sz: %i", out_sz);
   if(out_sz > xsock->swr_queue.capacity - xsock->swr_queue.size) // pitty, not enough space in queue
   {
      _fill_error(xsock, XS_EFILTER, "not enough space in queue");
      goto release;
   }
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

      occam_log_d(xsock->log, "write %i bytes to output queue", written);
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
   occam_logs_d(xsock->log, "xmpp socket connection failed");
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
   xsock->srd_buf = occam_alloc(xsock->mem, xsock->settings.rd_queue_size);
   if(!xsock->srd_buf)
   {
      _fill_error(xsock, XS_EALLOCATION, "failed to allocate read buffer");
      goto abort;
   }
   xsock->swr_buf = occam_alloc(xsock->mem, xsock->settings.wr_queue_size);
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
      occam_free(xsock->mem, xsock->srd_buf);
   if(xsock->swr_buf)
      occam_free(xsock->mem, xsock->swr_buf);
}

static int _update_queues(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   if(xsock->srd_queue.capacity == xsock->settings.rd_queue_size
      && xsock->swr_queue.capacity == xsock->settings.wr_queue_size)
      return XS_OK;
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
      occam_logs_d(xsock->log, "socket creation failed");
      goto abort;
   }

   char addr[255];
   if(TS_NO_ERROR != tinsock_connect(xsock->sock, SOCKADDR(&xsock->settings.addr),
                                     _addr_size_by_family(xsock->settings.addr.ss_family)))
   {
      _fill_sock_error(xsock, XS_ETRANSMISSION, "failed to connect");
      occam_log_d(xsock->log, "failed to connect to %s:%i", 
         tinsock_inet_ntop(xsock->settings.addr.ss_family, SOCKADDR_IP(&xsock->settings.addr), addr, 255),
         SOCKADDR_PORT(&xsock->settings.addr));
      goto error1;
   }
   tinsock_fcntl(xsock->sock, TS_F_SETFL, TS_O_NONBLOCK);
   occam_log_d(xsock->log, "connected to %s:%i", 
      tinsock_inet_ntop(xsock->settings.addr.ss_family, SOCKADDR_IP(&xsock->settings.addr), addr, 255),
      SOCKADDR_PORT(&xsock->settings.addr));
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

static int _write_xmpp_data(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   //TODO: make an error
   assert(xsock->settings.pair_jid);

   if(xsock->srd_queue.size == 0)
      return XS_OK;
   int res = XS_OK;

   xmpp_stanza_t *reply = xmpp_stanza_new(xsock->xmppctx);
   xmpp_stanza_t *body = xmpp_stanza_new(xsock->xmppctx);
   xmpp_stanza_t *text = xmpp_stanza_new(xsock->xmppctx);
   void * out_buf = NULL;
   if(!reply || !body || !text)
   {
      res = XS_ERROR;
      goto cleanup;
   }

   xmpp_stanza_set_name(reply, "message");
   xmpp_stanza_set_type(reply, "chat");
   xmpp_stanza_set_attribute(reply, "to", xsock->settings.pair_jid);

   xmpp_stanza_set_name(body, "body");

   int sz;
   const void * buf = cbuffer_seq_avail_read(&xsock->srd_queue, xsock->srd_buf, &sz);

   size_t out_sz;
   void * state = xsock->settings.rd_filter.init_state(buf, sz, &out_sz);
   out_buf = occam_alloc(xsock->mem, out_sz);
   int cons, wr;
   if(!out_buf || xsock->settings.rd_filter.filter(buf, sz, out_buf, out_sz, &cons, &wr, state) || cons != sz)
   {
      xsock->settings.rd_filter.deinit_state(state);
      res = XS_ERROR;
      goto cleanup;
   }
   xsock->settings.rd_filter.deinit_state(state);

   xmpp_stanza_set_text_with_size(text, out_buf, wr);
   cbuffer_read(&xsock->srd_queue, sz);
   xmpp_stanza_add_child(body, text);
   xmpp_stanza_add_child(reply, body);

   xmpp_send(xsock->xmppconn, reply);

cleanup:
   if(text)
      xmpp_stanza_release(text);
   if(body)
      xmpp_stanza_release(body);
   if(reply)
      xmpp_stanza_release(reply);
   if(out_buf)
      occam_free(xsock->mem, out_buf);
   return res;
}

XMPPSOCKET_FUNCTION(int, run_once)(XMPPSOCKET_ITEM(socket_t) * xsock)
{
   PRECONDITION(xsock->sock != TS_SOCKET_ERROR, xsock);
   PRECONDITION(xsock->xmppconn != NULL, xsock);

   occam_logs_t(xsock->log, "run_once");

   clock_t cur_tm = clock();
   clock_t dt = _msecs_passed(xsock, cur_tm);
   if(dt > xsock->settings.latency || xsock->srd_queue.size > xsock->srd_queue.capacity/2)
   {
      if(_write_xmpp_data(xsock) != XS_OK)
         return XS_ERROR;
      xsock->last_tm = cur_tm;
   }

   if(xsock->srd_queue.size == 0)
   {
      dt = 10000;
      xsock->last_tm = cur_tm;
   }
   else
   {
      dt = xsock->settings.latency - dt;
      if(dt < 0)
         dt = 0;
   }

   int need_write = (xsock->swr_queue.size != 0);
   xmpp_run_once_foreign(xsock->xmppctx, dt, &xsock->sock, 1, need_write ? &xsock->sock : NULL, need_write ? 1 : 0);
   occam_logs_t(xsock->log, "exited");

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
            occam_logs_t(xsock->log, "sock closed?");
            //TODO: socket closed?
         }
         else if(res != -1)
         {
            cbuffer_write(&xsock->srd_queue, res);
            occam_log_d(xsock->log, "read %i bytes from socket", res);
         }
      }
   }
   //write to socket
   {
      int sz;
      const void * buf;
      buf = cbuffer_seq_avail_read(&xsock->swr_queue, xsock->swr_buf, &sz);
      if(sz)
      {
         occam_log_d(xsock->log, "have %i bytes to write", sz);
         int res = tinsock_write(xsock->sock, buf, sz);
         if(res == -1 && !tinsock_is_recoverable())
         {
            _fill_sock_error(xsock, XS_ETRANSMISSION, "error writing to socket");
            return XS_ERROR;
         }
         else if(res == 0)
         {
            occam_logs_t(xsock->log, "sock closed?");
            //TODO: socket closed?
         }
         else if(res != -1)
         {
            cbuffer_read(&xsock->swr_queue, res);
            occam_log_d(xsock->log, "written %i bytes to socket", res);
         }
      }
   }
   return XS_OK;
}

static void* _init_dummy_state(const void * data, int sz, size_t * out_sz)
{
   if(out_sz)
      *out_sz = sz;
   return NULL;
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
