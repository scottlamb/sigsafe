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

int main(void) {
    char buf[42];
    int retval;

    sigsafe_install_handler(SIGUSR1, NULL);
    sigsafe_install_tsd(0, NULL);
    /*raise(SIGUSR1);*/
    memset(buf, 0, sizeof buf);
    retval = sigsafe_read(0, buf, sizeof(buf)-1);
    printf("read returned: %d\n", retval);
    if (retval >= 0) {
        printf("buf: %s\n", buf);
    } else {
        printf("error condition %d (%s)\n", -retval, strerror(-retval));
    }
    return 0;
}
