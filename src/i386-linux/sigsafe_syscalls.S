/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#include <asm/unistd.h>
#include <asm/errno.h>

/*
 * __NR_select actually refers to an older version that takes a structure.
 * We want the not-obsolete one.
 /
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

.comm sigsafe_key,4

#define SYSCALL(name, args)                                             ;\
.text                                                                   ;\
.type sigsafe_##name,@function                                          ;\
LABEL(sigsafe_##name)                                                   ;\
        pushl   sigsafe_key                                             ;\
        call    pthread_getspecific                                     ;\
        pop     %ecx /* not used */                                     ;\
        SAVE_REGS_##args                                                ;\
        SETUP_ARGS_##args                                               ;\
        testl   %eax,%eax                                               ;\
        je      L_sigsafe_##name##_nocompare                            ;\
LABEL(_sigsafe_##name##_minjmp)                                         ;\
        cmp     $0,(%eax)                                               ;\
        jne     _sigsafe_##name##_jmpto                                 ;\
L_sigsafe_##name##_nocompare:                                           ;\
        movl    $__NR_##name,%eax                                       ;\
LABEL(_sigsafe_##name##_maxjmp)                                         ;\
        int     $0x80                                                   ;\
        RESTORE_REGS_##args                                             ;\
        ret                                                             ;\
LABEL(_sigsafe_##name##_jmpto)                                          ;\
        movl    $-EINTR,%eax                                            ;\
        RESTORE_REGS_##args                                             ;\
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
        pop %ebx                                                            ;\
        RESTORE_REGS_0
#define RESTORE_REGS_2 _RESTORE_REGS_2
#define _RESTORE_REGS_2                                                     ;\
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

/*
 * SETUP_ARGS_##nargs puts the arguments into the register form the system
 * call expects. SAVE_REGS_##nargs will have just been called.
 */
#define SETUP_ARGS_0
#define SETUP_ARGS_1 _SETUP_ARGS_1(0)
#define _SETUP_ARGS_1(off)                                                  ;\
        movl    0x08+off(%ebx),%ebx                                         ;\
        /*      0x04+off(%ebx) contains our return address */
#define SETUP_ARGS_2 _SETUP_ARGS_2(0)
#define _SETUP_ARGS_2(off)                                                  ;\
        movl    0x0c+off(%ebx),%ecx                                         ;\
        _SETUP_ARGS_1(off)
#define SETUP_ARGS_3 _SETUP_ARGS_3(0)
#define _SETUP_ARGS_3(off)                                                  ;\
        movl    0x10+off(%ebx),%edx                                         ;\
        _SETUP_ARGS_2(off)
#define SETUP_ARGS_4 _SETUP_ARGS_4(0)
#define _SETUP_ARGS_4(off)                                                  ;\
        movl    0x18+off(%ebx),%esi                                         ;\
        _SETUP_ARGS_3(off+4)                                                ;\
        /*      0x04+off(%ebx) contains our saved %esi */
#define SETUP_ARGS_5 _SETUP_ARGS_5(0)
#define _SETUP_ARGS_5(off)                                                  ;\
        movl    0x20(%ebx),%edi                                             ;\
        _SETUP_ARGS_4(off+4)                                                ;\
        /*      0x04+off(%ebx) contains our saved %edi */

#include "syscalls.h"
