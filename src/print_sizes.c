/** @file
 * Prints the sizes of various C types; helps when writing assembly code.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <setjmp.h>
#include "sigsafe.h"
#include "sigsafe_internal.h"

#define mysizeof(x) ((unsigned long) sizeof(x))

int main(void) {
    printf("sizeof(sigset_t) == %lu\n", mysizeof(sigset_t));
    printf("sizeof(pthread_key_t) == %lu\n", mysizeof(pthread_key_t));
    printf("sizeof(sig_atomic_t) == %lu\n", mysizeof(sig_atomic_t));
    printf("sizeof(jmp_buf) == %lu\n", mysizeof(jmp_buf));
    printf("sizeof(void*) == %lu\n", mysizeof(void**));
    printf("sizeof(struct sigsafe_tsd_) == %lu\n",
           mysizeof(struct sigsafe_tsd_));
    return 0;
}
