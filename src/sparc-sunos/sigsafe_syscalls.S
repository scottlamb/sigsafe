/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#define _ASM
#include <sys/trap.h>
#include <sys/syscall.h>
#include <sys/errno.h>

.comm sigsafe_key, 4

#define SYSCALL(name,args) \
.text                                                               ;\
.global sigsafe_##name                                              ;\
.type sigsafe_##name,#function                                      ;\
sigsafe_##name:                                                     ;\
        save    %sp,-96,%sp                                         ;\
        set     sigsafe_key, %g1                                    ;\
        call    pthread_getspecific                                 ;\
        ld      [%g1], %o0                                          ;\
        mov     %o0, %g2                                            ;\
        restore                                                     ;\
$sigsafe_##name##_check:                                            ;\
        cmp     %g2, 0                                              ;\
        be      _sigsafe_##name##_maxjmp                            ;\
        nop                                                         ;\
LABEL(_sigsafe_##name##_minjmp)                                     ;\
        ld      [%g2], %g1                                          ;\
        cmp     %g1, %g0                                            ;\
        bne     _sigsafe_##name##_jmpto                             ;\
        mov     SYS_##name, %g1                                     ;\
LABEL(_sigsafe_##name##_maxjmp)                                     ;\
        ta      ST_SYSCALL                                          ;\
        bcc     $sigsafe_##name##_out                               ;\
        cmp     %o0, ERESTART                                       ;\
        be      $sigsafe_##name##_check                             ;\
        nop                                                         ;\
        retl                                                        ;\
        neg     %o0                                                 ;\
LABEL(_sigsafe_##name##_jmpto)                                      ;\
        mov     -EINTR, %o0                                         ;\
$sigsafe_##name##_out:                                              ;\
        retl                                                        ;\
        nop                                                         ;\
.size sigsafe_##name, . - sigsafe_##name

#define LABEL(label) .global label; label:

#include "syscalls.h"
