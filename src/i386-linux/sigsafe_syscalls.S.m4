/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#include <asm/unistd.h>
#include <asm/errno.h>

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

changequote([, ])

define(POPARGS_0, [])
define(POPARGS_1, [dnl
        pop     %ebx
])
define(POPARGS_2, [POPARGS_1])
define(POPARGS_3, [POPARGS_2])
define(POPARGS_4, [dnl
        pop     %edi
POPARGS_3])
define(POPARGS_5, [dnl
        pop     %esi
POPARGS_4])

define(SYSCALL, [
.text
.type sigsafe_$1,@function
.global sigsafe_$1
sigsafe_$1:
        pushl   sigsafe_key
        call    pthread_getspecific
        pop     %ecx
ifelse($2, 0, [dnl No arguments => no work
], [dnl More arguments => stack math in EBX
        push    %ebx
ifelse(eval($2 > 3), 1, [dnl
        push    %esi
], [])dnl
ifelse(eval($2 > 4), 1, [dnl
        push    %edi
], [])dnl
        movl    %esp,%ebx
ifelse($2, 1, [dnl
        movl    0x08(%ebx),%ebx
], [])dnl
ifelse($2, 2, [dnl
        movl    0x0c(%ebx),%ecx
        movl    0x08(%ebx),%ebx
], [])dnl
ifelse($2, 3, [dnl
        movl    0x10(%ebx),%edx
        movl    0x0c(%ebx),%ecx
        movl    0x08(%ebx),%ebx
], [])dnl
ifelse($2, 4, [dnl
        movl    0x14(%ebx),%esi
        movl    0x10(%ebx),%edx
        movl    0x0c(%ebx),%ecx
        movl    0x08(%ebx),%ebx
], [])dnl
ifelse($2, 5, [dnl
        movl    0x18(%ebx),%edi
        movl    0x14(%ebx),%edi
        movl    0x10(%ebx),%edx
        movl    0x0c(%ebx),%ecx
        movl    0x08(%ebx),%ebx
], [])])dnl
        testl   %eax,%eax
        je      L_sigsafe_$1_nocompare
.global _sigsafe_$1_minjmp
_sigsafe_$1_minjmp:
        cmp     $[]0,(%eax)
        jne     _sigsafe_$1_jmpto
L_sigsafe_$1_nocompare:
        movl    $__NR_$1,%eax
.global _sigsafe_$1_maxjmp
_sigsafe_$1_maxjmp:
        int     $[]0x80
POPARGS_$2
        ret
.global _sigsafe_$1_jmpto
_sigsafe_$1_jmpto:
        movl    $-EINTR,%eax
POPARGS_$2
        ret
.size sigsafe_$1, . - sigsafe_$1
])

include([syscalls.h])
