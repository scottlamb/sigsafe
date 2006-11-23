/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#include <sys/syscall.h>
#include <sigsafe_config.h>

#define EINTR 4 /* from sys/errno.h> */
#define SYS_sigsuspend SYS___sigsuspend14

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
        int     $0x80                                                   ;\
        jc      L_sigsafe_##name##_error  /* if carry flag set, error */;\
        ret                                                             ;\
LABEL(sigsafe_##name##_jmpto_)                                          ;\
        movl    $EINTR,%eax                                             ;\
L_sigsafe_##name##_error:                                               ;\
        neg     %eax                /* return -errno */                 ;\
        ret                                                             ;\
.size sigsafe_##name, . - sigsafe_##name

#define LABEL(label)                                                     \
.global label                                                           ;\
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
