/* $Id$ */

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

define(SYSCALL, [
.text
.type sigsafe_$1,@function
.global sigsafe_$1
sigsafe_$1:
        pushl   sigsafe_key
        call    pth$1_getspecific
        pop     %ecx
        push    %edi
        push    %ebx
        movl    %esp,%edi
        movl    0x0c(%edi),%ebx
        movl    0x10(%edi),%ecx
        movl    0x14(%edi),%edx
        testl   %eax,%eax
        je      nocompare
.global _sigsafe_$1_minjmp
_sigsafe_$1_minjmp:
        cmp     $0,(%eax)
        jne     _sigsafe_$1_jmpto
nocompare:
        movl    $__NR_$1,%eax
.global _sigsafe_$1_maxjmp
_sigsafe_$1_maxjmp:
        int     $0x80
        pop     %ebx
        pop     %edi
        ret
.global _sigsafe_$1_jmpto
_sigsafe_$1_jmpto:
        movl    $-EINTR,%eax
        pop     %ebx
        pop     %edi
        ret
.size sigsafe_$1, . - sigsafe_$1
])

include([syscalls.h])
