/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#include <sys/syscall.h>

#define EINTR 4 /* from sys/errno.h> */

.comm sigsafe_key,4

#define SYSCALL(name,args)                                               \
.text                                                                   ;\
.type sigsafe_##name,@function                                          ;\
LABEL(sigsafe_##name)                                                   ;\
        pushl   sigsafe_key                                             ;\
        call    pthread_getspecific                                     ;\
        pop     %ecx /* discarded */                                    ;\
        testl   %eax,%eax                                               ;\
        je      L_sigsafe_##name##_nocompare                            ;\
LABEL(_sigsafe_##name##_minjmp)                                         ;\
        cmp     $0,(%eax)                                               ;\
        jne     _sigsafe_##name##_jmpto                                 ;\
L_sigsafe_##name##_nocompare:                                           ;\
        movl    $SYS_##name,%eax                                        ;\
LABEL(_sigsafe_##name##_maxjmp)                                         ;\
        int     $0x80                                                   ;\
        jc      L_sigsafe_##name##_error  /* if carry flag set, error */;\
        ret                                                             ;\
LABEL(_sigsafe_##name##_jmpto)                                          ;\
        movl    $EINTR,%eax                                             ;\
L_sigsafe_##name##_error:                                               ;\
        neg     %eax                /* return -errno */                 ;\
        ret                                                             ;\
.size sigsafe_##name, . - sigsafe_##name

#define LABEL(label)                                                     \
.global label                                                           ;\
label:

#include "syscalls.h"
