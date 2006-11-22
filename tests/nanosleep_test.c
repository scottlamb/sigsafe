/*
 * On OS X, this is a Mach syscall, not like the others.
 */

#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <sigsafe.h>

int
main(int argc, char **argv)
{
    struct timespec ts = {.tv_sec = 60, .tv_nsec = 0 };
    int rv;

    sigsafe_install_handler(SIGUSR1, NULL);
    sigsafe_install_tsd(0, NULL);
    rv = sigsafe_nanosleep(&ts, NULL);
    printf("sigsafe_nanosleep returned %d\n", rv);
    if (rv < 0) {
        printf("(which is %s)\n", strerror(-rv));
    }
    return 0;
}
