/* $Id$ */

#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

/* TODO implement sigsafe_nanosleep */

pid_t sigsafe_wait(int *status) {
    return sigsafe_wait4(WAIT_ANY, status, 0, NULL);
}

pid_t sigsafe_wait3(int *status, int options, struct rusage *rusage) {
    return sigsafe_wait4(WAIT_ANY, status, options, rusage);
}
