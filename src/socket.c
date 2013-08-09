#include <libstrophe/strophe.h>
#include <libtinysocket/src/tinysocket.h>

int main(int argc, char ** argv)
{
   tinsock_socket_t sock = tinsock_socket(TS_AF_INET, TS_SOCK_STREAM, TS_IPPROTO_UNSPECIFIED);

   tinsock_close(sock);
}
