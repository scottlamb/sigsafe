/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#include <asm/unistd.h>
#include <asm/errno.h>

#ifdef __DYNAMIC__
#error "This code is position-dependent."
#endif

/*
 * __NR_select actually refers to an older version that takes a structure.
 * We want the not-obsolete one.
 */
#undef __NR_select
#define __NR_select __NR__newselect

/*
 * RedHat 9 has the epoll header but does not define __NR_epoll; most
 * annoying. Work around by including it here.
 */
#ifndef __NR_epoll_wait
#define __NR_epoll_wait 256
#endif

/*
 * int 0x80 form of syscall:
 * register  kernel syscall expectation          gcc return expectation
 * %eax      syscall                             return value
 * %ebx      arg 1                               preserve
 * %ecx      arg 2                               we may clobber
 * %edx      arg 3                               we may clobber
 * %esi      arg 4                               preserve
 * %edi      arg 5                               preserve
 */

#define SYSCALL(name, args)                                             ;\
.text                                                                   ;\
.type sigsafe_##name,@function                                          ;\
.globl sigsafe_##name                                                   ;\
sigsafe_##name:                                                         ;\
        LOAD_TSD                                                        ;\
        SAVE_REGS_##args                                                ;\
        COPY_STACK_PTR_##args                                           ;\
        SETUP_ARGS_##args                                               ;\
        testl   %eax,%eax                                               ;\
        je      L_sigsafe_##name##_nocompare                            ;\
HIDDEN(sigsafe_##name##_minjmp_)                                        ;\
        cmp     $0,(%eax)                                               ;\
        jne     sigsafe_##name##_jmpto_                                 ;\
L_sigsafe_##name##_nocompare:                                           ;\
        movl    $__NR_##name,%eax                                       ;\
HIDDEN(sigsafe_##name##_maxjmp_)                                        ;\
        int     $0x80                                                   ;\
        RESTORE_REGS_##args                                             ;\
        ret                                                             ;\
HIDDEN(sigsafe_##name##_jmpto_)                                         ;\
        movl    $-EINTR,%eax                                            ;\
        RESTORE_REGS_##args                                             ;\
        ret                                                             ;\
.size sigsafe_##name, . - sigsafe_##name

#define HIDDEN(label) .internal label; .globl label; label:

/*
 * Since we're using the registers above for system call arguments, we
 * overwrite them. SAVE_REGS_#nargs and RESTORE_REGS_##nargs do the necessary
 * saving and restoring to honor our obligations as a callee.
 */

#define SAVE_REGS_0
#define SAVE_REGS_1                                                         ;\
        SAVE_REGS_0                                                         ;\
        push %ebx
#define SAVE_REGS_2 _SAVE_REGS_2
#define _SAVE_REGS_2                                                        ;\
        SAVE_REGS_1                                                         ;\
        /* %ecx we may clobber */
#define SAVE_REGS_3                                                         ;\
        SAVE_REGS_2                                                         ;\
        /* %edx we may clobber */
#define SAVE_REGS_4                                                         ;\
        SAVE_REGS_3                                                         ;\
        push %esi
#define SAVE_REGS_5                                                         ;\
        SAVE_REGS_4                                                         ;\
        push %edi

#define RESTORE_REGS_0
#define RESTORE_REGS_1                                                      ;\
        pop %ebx
#define RESTORE_REGS_2                                                      ;\
        /* %ecx we may clobber */                                           ;\
        RESTORE_REGS_1
#define RESTORE_REGS_3                                                      ;\
        /* %edx we may clobber */                                           ;\
        RESTORE_REGS_2
#define RESTORE_REGS_4                                                      ;\
        pop %esi                                                            ;\
        RESTORE_REGS_3
#define RESTORE_REGS_5                                                      ;\
        pop %edi                                                            ;\
        RESTORE_REGS_4

/**
 * Copies the stack pointer to %ebx so we can do math on it.
 * Not necessary for zero-argument system call.
 */
/*@{*/
#define COPY_STACK_PTR_0
#define COPY_STACK_PTR_1 movl %esp,%ebx
#define COPY_STACK_PTR_2 COPY_STACK_PTR_1
#define COPY_STACK_PTR_3 COPY_STACK_PTR_1
#define COPY_STACK_PTR_4 COPY_STACK_PTR_1
#define COPY_STACK_PTR_5 COPY_STACK_PTR_1
/*@}*/

/**
 * SETUP_ARGS_##nargs puts the arguments into the register form the system
 * call expects. SAVE_REGS_##nargs and COPY_STACK_PTR_##nargs should have been
 * called.
 */
/*@{*/
#define SETUP_ARGS_0                                                         \
#define SETUP_ARGS_1 _SETUP_ARGS_1(0)
#define _SETUP_ARGS_1(off)                                                   \
        movl    0x08+off(%ebx),%ebx                                         ;\
        /*      0x04+off(%ebx) contains our return address */               ;\
        /*      0x00+off(%ebx) contains our saved %ebx */
#define SETUP_ARGS_2 _SETUP_ARGS_2(0)
#define _SETUP_ARGS_2(off)                                                   \
        movl    0x0c+off(%ebx),%ecx                                         ;\
        _SETUP_ARGS_1(off)
#define SETUP_ARGS_3 _SETUP_ARGS_3(0)
#define _SETUP_ARGS_3(off)                                                   \
        movl    0x10+off(%ebx),%edx                                         ;\
        _SETUP_ARGS_2(off)
#define SETUP_ARGS_4 _SETUP_ARGS_4(0)
#define _SETUP_ARGS_4(off)                                                   \
        movl    0x18+off(%ebx),%esi                                         ;\
        _SETUP_ARGS_3(off+4)                                                ;\
        /*      0x00+off(%ebx) contains our saved %esi */
#define SETUP_ARGS_5 _SETUP_ARGS_5(0)
#define _SETUP_ARGS_5(off)                                                   \
        movl    0x20(%ebx),%edi                                             ;\
        _SETUP_ARGS_4(off+4)                                                ;\
        /*      0x00+off(%ebx) contains our saved %edi */
/*@}*/

#ifdef _THREAD_SAFE
#define LOAD_TSD \
        pushl   sigsafe_key_                                            ;\
        call    pthread_getspecific                                     ;\
        pop     %ecx /* not used */
#else
#define LOAD_TSD \
        movl    sigsafe_data_,%eax
#endif

.internal sigsafe_socketcall
#include "syscalls.h"
