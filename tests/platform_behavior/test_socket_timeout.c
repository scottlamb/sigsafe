/**
 * @file
 * Tests if <tt>setsockopt(..., SO_RCVTIMEO, ...)</tt> and
 * <tt>setsockopt(..., SO_SNDTIMEO, ...)</tt> support is available.
 * This is the most straightforward and efficient way to implement timeouts in
 * code waiting for network input. (And doesn't require signal usage at all.)
 * SUSv3 notes <a
 * href="http://www.opengroup.org/onlinepubs/007904975/functions/setsockopt.html">here</a>
 * that "not all implementations all this option to be set" in
 * both cases.
 * @legal
 * Copyright &copy; 2004 &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

int error_wrap(int retval, const char *funcname) {
    if (retval < 0) {
        fprintf(stderr, "%s returned %d (errno==%d) (%s)\n",
                funcname, retval, errno, strerror(errno));
        abort();
    }
    return retval;
}

int main(void) {
    struct sockaddr_in server_addr;
    int server_addr_len = sizeof(struct sockaddr_in);
    int listen_fd, connected_fd;
    int retval;
    struct timeval timeout;

    /*
     * I want TCP sockets rather than UNIX ones. socketpair(2) doesn't support
     * PF_INET on most systems, so I explicitly connect the sockets instead.
     * A lot of setup work, but oh well.
     */
    listen_fd = error_wrap(socket(PF_INET, SOCK_STREAM, 0), "socket");
    connected_fd = error_wrap(socket(PF_INET, SOCK_STREAM, 0), "socket");
    error_wrap(listen(listen_fd, 5), "listen");
    if (error_wrap(fork(), "fork") == 0) {
        struct sockaddr_in originating_addr;
        int originating_addr_len = sizeof(struct sockaddr_in);
        int accepted_fd;
        char c;

        /* Accept the connection, wait for it to be closed, then die. */
        accepted_fd = error_wrap(accept(listen_fd,
                                        (struct sockaddr*) &originating_addr,
                                        &originating_addr_len), "accept");
        retval = error_wrap(read(accepted_fd, &c, 1), "read");
        assert(retval == 0);
        return 0;
    }
    error_wrap(getsockname(listen_fd, (struct sockaddr*) &server_addr,
                           &server_addr_len), "getsockname");
    error_wrap(connect(connected_fd, (struct sockaddr*) &server_addr,
                       server_addr_len), "connect");

    /*
     * Okay, now here's the actual test.
     */
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    error_wrap(setsockopt(connected_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                          sizeof(struct timeval)),
               "setsockopt(..., SO_RCVTIMEO, ...)");
    error_wrap(setsockopt(connected_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                          sizeof(struct timeval)),
               "setsockopt(..., SO_SNDTIMEO, ...)");
    printf("Good; SO_RCVTIMEO and SO_SNDTIMEO seemed to take.\n");
    return 0;
}
