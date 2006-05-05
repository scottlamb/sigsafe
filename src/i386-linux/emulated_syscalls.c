/** @file
 * Emulated system calls on i386-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#define _GNU_SOURCE
#include "sigsafe_internal.h"
#include <linux/net.h>

PRIVATE_DEF(int sigsafe_socketcall(int call, unsigned long *args));

int sigsafe_accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
    unsigned long args[] = { s, (long) addr, (long) addrlen };
    return sigsafe_socketcall(SYS_ACCEPT, args);
}

int sigsafe_connect(int s, const struct sockaddr *name, socklen_t namelen) {
    unsigned long args[] = { s, (long) name, namelen };
    return sigsafe_socketcall(SYS_CONNECT, args);
}

ssize_t sigsafe_recv(int s, void *buf, size_t len, int flags) {
    unsigned long args[] = { s, (long) buf, len, flags };
    return sigsafe_socketcall(SYS_RECV, args);
}

ssize_t sigsafe_recvfrom(int s, void *buf, size_t len, int flags,
                     struct sockaddr *from, socklen_t *fromlen) {
    unsigned long args[] = { s, (long) buf, len, flags, (long) from,
                             (long) fromlen };
    return sigsafe_socketcall(SYS_RECVFROM, args);
}

ssize_t sigsafe_recvmsg(int s, struct msghdr *msg, int flags) {
    unsigned long args[] = { s, (long) msg, flags };
    return sigsafe_socketcall(SYS_RECVMSG, args);
}

ssize_t sigsafe_send(int s, const void *buf, size_t len, int flags) {
    unsigned long args[] = { s, (long) buf, len, flags };
    return sigsafe_socketcall(SYS_SEND, args);
}

ssize_t sigsafe_sendto(int s, const void *buf, size_t len, int flags,
                       const struct sockaddr *to, socklen_t tolen) {
    unsigned long args[] = { s, (long) buf, len, flags, (long) to,
                             (long) tolen };
    return sigsafe_socketcall(SYS_SENDTO, args);
}

ssize_t sigsafe_sendmsg(int s, const struct msghdr *msg, int flags) {
    unsigned long args[] = { s, (long) msg, flags };
    return sigsafe_socketcall(SYS_SENDMSG, args);
}
