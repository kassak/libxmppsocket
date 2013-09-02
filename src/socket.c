#include <libstrophe/strophe.h>
#include "xmpp_socket.h"
#include <assert.h>
#include <stdlib.h>
#include <limits.h>

int main(int argc, char ** argv)
{
   xmppsock_init();
   xmppsock_socket_t * xsock = xmppsock_create((occam_allocator_t*)NULL, xmpp_get_default_logger(OCCAM_LOG_LEVEL_DEBUG));

   xmppsock_settings_t * s = xmppsock_settings(xsock);
   s->jid = argv[1];
   s->pass = argv[2];

   int res = xmppsock_connect_xmpp(xsock);
   if(res != XS_OK)
   {
      perror("cxmpp");
      assert(res == XS_OK);
   }
   errno = 0;
   char * ss;
   unsigned long port = strtoul(argv[4], &ss, 10);
   if((errno == ERANGE && port == ULONG_MAX) || (errno != 0 && port == 0) || ss == argv[4])
   {
      fprintf(stderr, "unknown port %s\n", argv[4]);
      abort();
   }
   tinsock_sockaddr_storage_t stor;
   if(1 == tinsock_inet_pton(TS_AF_INET6, argv[3], &((tinsock_sockaddr_in6_t*)&stor)->sin6_addr))
   {
      stor.ss_family = TS_AF_INET6;
      ((tinsock_sockaddr_in6_t*)&stor)->sin6_port = port;
   }
   else if(1 == tinsock_inet_pton(TS_AF_INET, argv[3], &((tinsock_sockaddr_in_t*)&stor)->sin_addr))
   {
      stor.ss_family = TS_AF_INET;
      ((tinsock_sockaddr_in_t*)&stor)->sin_port = port;
   }
   else
   {
      fprintf(stderr, "unknown address %s\n", argv[3]);
      abort();
   }

   s->addr = stor;
   res = xmppsock_connect_sock(xsock);
   if(res != XS_OK)
   {
      char buf[255];
      xmppsock_format_error(xmppsock_last_error(xsock), buf, 255);
      fprintf(stderr, "con sock: %s\n", buf);
      assert(res == XS_OK);
   }

   printf("done\n");

   xmppsock_dispose(xsock);
   xmppsock_deinit();
}
