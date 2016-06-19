#ifndef _STARTUP_H
#define _STARTUP_H

#include "myunp.h"

#define SA struct sockaddr

int
Tcp_connect(const char *host, const char *serv);

int
Tcp_listen(const char *host, const char *serv, socklen_t *addrlenp);

int
Udp_client(const char *host, const char *serv, SA **saptr, socklen_t *lenptr);

int
Udp_server(const char *host, const char *serv, socklen_t *addrlenp);

#endif
