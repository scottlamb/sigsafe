/* $Id$ */

#define _GNU_SOURCE
#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <linux/net.h>

int sigsafe_socketcall(int call, unsigned long *args);

int sigsafe_accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
    unsigned long args[] = { s, (long) addr, (long) addrlen };
    return sigsafe_socketcall(SYS_ACCEPT, args);
}

int sigsafe_connect(int s, const struct sockaddr *name, socklen_t namelen) {
    unsigned long args[] = { s, (long) name, namelen };
    return sigsafe_socketcall(SYS_CONNECT, args);
}
