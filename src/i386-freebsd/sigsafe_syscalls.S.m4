/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#include <sys/syscall.h>

#define EINTR 4 /* from sys/errno.h> */

.comm sigsafe_key,4

changequote([, ])

define(SYSCALL, [
.text
.type sigsafe_$1,@function
.global sigsafe_$1
sigsafe_$1:
        pushl   sigsafe_key
        call    pthread_getspecific
        pop     %ecx /* discarded */
        testl   %eax,%eax
        je      L_sigsafe_$1_nocompare
.global _sigsafe_$1_minjmp
_sigsafe_$1_minjmp:
        cmp     $[]0,(%eax)
        jne     _sigsafe_$1_jmpto
L_sigsafe_$1_nocompare:
        movl    $SYS_$1,%eax
.global _sigsafe_$1_maxjmp
_sigsafe_$1_maxjmp:
        int     $[]0x80
        jc      L_sigsafe_$1_error  /* if carry flag set, error */
        ret
L_sigsafe_$1_error:
        neg     %eax                /* return -errno */
        ret
.global _sigsafe_$1_jmpto
_sigsafe_$1_jmpto:
        movl    $-EINTR,%eax
        ret
.size sigsafe_$1, . - sigsafe_$1
])

include([syscalls.h])
