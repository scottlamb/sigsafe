/** @file
 * Prints the sizes of various C types; helps when writing assembly code.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <setjmp.h>

int main(void) {
    printf("sizeof(sigset_t) == %zu\n", sizeof(sigset_t));
    printf("sizeof(pthread_key_t) == %zu\n", sizeof(pthread_key_t));
    printf("sizeof(sig_atomic_t) == %zu\n", sizeof(sig_atomic_t));
    printf("sizeof(jmp_buf) == %zu\n", sizeof(jmp_buf));
    printf("sizeof(intptr_t) == %zu\n", sizeof(intptr_t));
    return 0;
}
