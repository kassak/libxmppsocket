#include <libstrophe/strophe.h>
#include "xmpp_socket.h"
#include <assert.h>

int main(int argc, char ** argv)
{
   xmppsock_init();
   xmppsock_socket_t * xsock = xmppsock_create(NULL, XS_XL_DEBUG);

   xmppsock_settings_t * s = xmppsock_settings(xsock);
   s->jid = "";
   s->pass = "";

   int res = xmppsock_connect_xmpp(xsock);
   assert(res == XS_OK);

   printf("done\n");

   xmppsock_dispose(xsock);
   xmppsock_deinit();
}
