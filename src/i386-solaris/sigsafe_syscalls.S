/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#define _ASM
#include <sys/syscall.h>
#include <sys/errno.h>
#include <sigsafe_config.h>

#define SYSCALL(name,args)                                               \
.text                                                                   ;\
.type sigsafe_##name,@function                                          ;\
LABEL(sigsafe_##name)                                                   ;\
        LOAD_TSD                                                        ;\
        testl   %eax,%eax                                               ;\
        je      L_sigsafe_##name##_nocompare                            ;\
LABEL(sigsafe_##name##_minjmp_)                                         ;\
        cmp     $0,(%eax)                                               ;\
        jne     sigsafe_##name##_jmpto_                                 ;\
L_sigsafe_##name##_nocompare:                                           ;\
        movl    $SYS_##name,%eax                                        ;\
LABEL(sigsafe_##name##_maxjmp_)                                         ;\
        lcall   $0x27,$0x0                                              ;\
        jb      L_sigsafe_##name##_error                                ;\
        ret                                                             ;\
LABEL(sigsafe_##name##_jmpto_)                                          ;\
        movl    $EINTR,%eax                                             ;\
L_sigsafe_##name##_error:                                               ;\
        cmp     $ERESTART,%eax                                          ;\
        je      sigsafe_##name                                          ;\
        neg     %eax                /* return -errno */                 ;\
        ret                                                             ;\
.size sigsafe_##name, . - sigsafe_##name

#define LABEL(label)                                                     \
.globl label                                                            ;\
label:

#ifdef _THREAD_SAFE
#define LOAD_TSD \
        pushl   sigsafe_key_                                            ;\
        call    pthread_getspecific                                     ;\
        pop     %ecx /* discarded */
#else
#define LOAD_TSD \
        mov     sigsafe_data_,%eax
#endif

#include "syscalls.h"
