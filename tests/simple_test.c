/** @file
 * Simple test of sigsafe.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <stdio.h>
#include <sigsafe.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

enum error_return_type {
    DIRECT,         /**< pthread functions */
    NEGATIVE,       /**< sigsafe functions */
    ERRNO           /**< old-school functions */
};

int error_wrap(int retval, const char *funcname, enum error_return_type type) {
    if (type == ERRNO && retval < 0) {
        fprintf(stderr, "%s returned %d (errno==%d) (%s)\n",
                funcname, retval, errno, strerror(errno));
        /*abort();*/
    } else if (type == DIRECT && retval != 0) {
        fprintf(stderr, "%s returned %d (%s)\n",
                funcname, retval, strerror(retval));
        /*abort();*/
    } else if (type == NEGATIVE && retval < 0) {
        fprintf(stderr, "%s returned %d (%s)\n",
                funcname, retval, strerror(-retval));
        /*abort();*/
    }
    return retval;
}


int main(void) {
    char buf[42];
    int retval;

    error_wrap(sigsafe_install_handler(SIGUSR1, NULL), "sigsafe_install_handler",
               NEGATIVE);
    error_wrap(sigsafe_install_tsd(0, NULL), "sigsafe_install_tsd", NEGATIVE);
    /*raise(SIGUSR1);*/
    error_wrap(retval = sigsafe_read(0, buf, sizeof(buf)-1), "sigsafe_read",
               NEGATIVE);
    if (retval >= 0) {
        buf[retval] = 0;
        printf("read %d bytes: <%s>\n", retval, buf);
    }
    sigsafe_clear_received();
    close(0);
    retval = sigsafe_read(0, buf, sizeof(buf)-1);
    if (retval != -EBADF) {
        printf("Incorrect error behavior. Wanted -EBADF, got %d\n",
               retval);
    }
    return 0;
}
