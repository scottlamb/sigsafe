/** @file
 * Adjusts instruction pointer as necessary on i386-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#define _GNU_SOURCE
#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <ucontext.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void sighandler_for_platform(ucontext_t *ctx) {
    struct sigsafe_syscall *s;
    void *eip;
    eip = (void*) ctx->uc_mcontext.gregs[REG_EIP];
    for (s = sigsafe_syscalls; s->address != NULL; s++) {
        if (s->minjmp <= eip && eip <= s->maxjmp) {
            ctx->uc_mcontext.gregs[REG_EIP] = (int) s->jmpto;
            return;
        }
    }
}
