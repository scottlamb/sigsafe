/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#include <asm/unistd.h>
#include <asm/errno.h>

#ifdef _THREAD_SAFE
.comm sigsafe_key,8
#else
.comm sigsafe_data,8
#endif

#define SYSCALL(name, args)                                             ;\
.text                                                                   ;\
.type sigsafe_##name,@function                                          ;\
LABEL(sigsafe_##name)                                                   ;\
        LOAD_TSD(args)                                                  ;\
        SETUP_ARGS_##args                                               ;\
        testq   %rax,%rax                                               ;\
        je      L_sigsafe_##name##_nocompare                            ;\
LABEL(_sigsafe_##name##_minjmp)                                         ;\
        cmp     $0,(%rax)                                               ;\
        jne     _sigsafe_##name##_jmpto                                 ;\
L_sigsafe_##name##_nocompare:                                           ;\
        movq    $__NR_##name,%rax                                       ;\
LABEL(_sigsafe_##name##_maxjmp)                                         ;\
        syscall                                                         ;\
        ret                                                             ;\
LABEL(_sigsafe_##name##_jmpto)                                          ;\
        movq    $-EINTR,%rax                                            ;\
        ret                                                             ;\
.size sigsafe_##name, . - sigsafe_##name

#define LABEL(label)                                                     \
.global label                                                           ;\
label:

/*
 * Since we're using the registers above for system call arguments, we
 * overwrite them. SAVE_REGS_#nargs and RESTORE_REGS_##nargs do the necessary
 * saving and restoring to honor our obligations as a callee.
 */

#define SAVE_REGS_0
#define SAVE_REGS_1 \
        SAVE_REGS_0                                                         ;\
        push %rbx
#define SAVE_REGS_2 \
        SAVE_REGS_1                                                         ;\
        push %rsi
#define SAVE_REGS_3 \
        SAVE_REGS_2                                                         ;\
        push %rdx
#define SAVE_REGS_4 \
        SAVE_REGS_3                                                         ;\
        push %rcx
#define SAVE_REGS_5 \
        SAVE_REGS_4                                                         ;\
        push %r8
#define SAVE_REGS_6 \
        SAVE_REGS_5                                                         ;\
        push %r9

#define RESTORE_REGS_0
#define RESTORE_REGS_1                                                      ;\
        pop %rbx
#define RESTORE_REGS_2                                                      ;\
        pop %rsi                                                            ;\
        RESTORE_REGS_1
#define RESTORE_REGS_3                                                      ;\
        pop %rdx                                                            ;\
        RESTORE_REGS_2
#define RESTORE_REGS_4                                                      ;\
        pop %rcx                                                            ;\
        RESTORE_REGS_3
#define RESTORE_REGS_5                                                      ;\
        pop %r8                                                             ;\
        RESTORE_REGS_4
#define RESTORE_REGS_6                                                      ;\
        pop %r9                                                             ;\
        RESTORE_REGS_5

/**
 * SETUP_ARGS_##nargs changes our function call-style arguments into kernel
 * syscall-style arguments.
 */
/*@{*/
#define SETUP_ARGS_0                                                         \
#define SETUP_ARGS_1
#define SETUP_ARGS_2
#define SETUP_ARGS_3
#define SETUP_ARGS_4 movq %rcx, %r10
#define SETUP_ARGS_5 SETUP_ARGS_4
#define SETUP_ARGS_6 SETUP_ARGS_5
/*@}*/

#ifdef _THREAD_SAFE
#define LOAD_TSD(args) \
        SAVE_ARGS_##args                                                    ;\
        movq    sigsafe_key, %rdi                                           ;\
        call    pthread_getspecific                                         ;\
        RESTORE_REGS_##args
#else
#define LOAD_TSD(args) \
        movq    sigsafe_data,%rax
#endif

#include "syscalls.h"
