#include <libstrophe/strophe.h>
#include "xmpp_socket.h"
#include <assert.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

void usage()
{
   fprintf(stderr, "Usage:\n"
      "\tU -- jid\n"
      "\tP -- pass\n"
      "\tR -- pair jid\n"
      "\td -- log level(possible TRACE, DEBUG, INFO, WARN, ERROR, NONE. default NONE)\n"
      "\ta -- IPv[4,6] address\n"
      "\tp -- port\n"
      "\tl -- listen(possible 0, 1. default 0)\n"
      "\th -- help\n"
      );
}

int parse_port(const char * s, uint16_t * port)
{
   errno = 0;
   char * ss;
   unsigned long pp = strtoul(s, &ss, 10);
   if((errno == ERANGE && pp == ULONG_MAX) || (errno != 0 && pp == 0) || ss == s)
      return 0;
   *port = pp;
   return 1;
}

int parse_log_level(const char * s, int * ll)
{
   if(0 == strcmp(s, "NONE"))
      *ll = OCCAM_LOG_LEVEL_NONE;
   else if(0 == strcmp(s, "TRACE"))
      *ll = OCCAM_LOG_LEVEL_TRACE;
   else if(0 == strcmp(s, "DEBUG"))
      *ll = OCCAM_LOG_LEVEL_DEBUG;
   else if(0 == strcmp(s, "INFO"))
      *ll = OCCAM_LOG_LEVEL_INFO;
   else if(0 == strcmp(s, "WARN"))
      *ll = OCCAM_LOG_LEVEL_WARN;
   else if(0 == strcmp(s, "ERROR"))
      *ll = OCCAM_LOG_LEVEL_ERROR;
   else
     return 0;
   return 1;
}

int parse_address(const char * s, tinsock_sockaddr_storage_t * addr)
{
   return tinsock_v4v6_inet_pton(s, addr);
}

int main(int argc, char ** argv)
{
   const char* jid = NULL;
   const char* pass = NULL;
   const char* pair_jid = NULL;
   int log_level = OCCAM_LOG_LEVEL_NONE;
   tinsock_sockaddr_storage_t addr = {0};
   uint16_t port = 0;
   int is_listening = 0;

   int op;
   while(-1 != (op = getopt(argc, argv, "U:P:R:d:a:p:l::h")))
   {
      switch(op)
      {
      case 'U':
         jid = optarg;
         break;
      case 'P':
         pass = optarg;
         break;
      case 'R':
         pair_jid = optarg;
         break;
      case 'd':
         if(!parse_log_level(optarg, &log_level))
         {
            fprintf(stderr, "Error: failed to parse log level.\n");
            usage();
            exit(1);
         }
         break;
      case 'a':
         if(!parse_address(optarg, &addr))
         {
            fprintf(stderr, "Error: failed to parse address.\n");
            usage();
            exit(1);
         }
         break;
      case 'p':
         if(!parse_port(optarg, &port))
         {
            fprintf(stderr, "Error: failed to parse port.\n");
            usage();
            exit(1);
         }
         break;
      case 'l':
         if(optarg)
            is_listening = (optarg[0] == '0') ? 0 : 1;
         else
            is_listening = 1;
         break;
      case 'h':
         usage();
         exit(0);
         break;
      case '?':
         usage();
         exit(1);
         break;
      }
   }

   //TODO: handle missed options;
   tinsock_v4v6_set_port(&addr, tinsock_htons(port));

   xmppsock_init();
   xmppsock_socket_t * xsock = xmppsock_create((occam_allocator_t*)NULL,
          log_level == OCCAM_LOG_LEVEL_NONE ? NULL : xmpp_get_default_logger(log_level));

   xmppsock_settings_t * s = xmppsock_settings(xsock);
   s->jid = jid;
   s->pass = pass;
   s->pair_jid = pair_jid;

   int res = xmppsock_connect_xmpp(xsock);
   if(res != XS_OK)
   {
      perror("cxmpp");
      assert(res == XS_OK);
   }


   s->addr = addr;
   res = xmppsock_connect_sock(xsock);
   if(res != XS_OK)
   {
      char buf[255];
      xmppsock_format_error(xmppsock_last_error(xsock), buf, 255);
      fprintf(stderr, "con sock: %s\n", buf);
      assert(res == XS_OK);
   }

   while(XS_OK == xmppsock_run_once(xsock))
      ;//printf(".\n");


   printf("done\n");

   xmppsock_dispose(xsock);
   xmppsock_deinit();
}
