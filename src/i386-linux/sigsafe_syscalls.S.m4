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

.text
.type sigsafe_read,@function
.global sigsafe_read
sigsafe_read:
        pushl   sigsafe_key
        call    pthread_getspecific
        pop     %ecx
        push    %edi
        push    %ebx
        movl    %esp,%edi
        movl    0x0c(%edi),%ebx
        movl    0x10(%edi),%ecx
        movl    0x14(%edi),%edx
        testl   %eax,%eax
        je      nocompare
.global _sigsafe_read_minjmp
_sigsafe_read_minjmp:
        cmp     $0,(%eax)
        jne     _sigsafe_read_jmpto
nocompare:
        movl    $__NR_read,%eax
.global _sigsafe_read_maxjmp
_sigsafe_read_maxjmp:
        int     $0x80
        pop     %ebx
        pop     %edi
        ret
.global _sigsafe_read_jmpto
_sigsafe_read_jmpto:
        movl    $-EINTR,%eax
        pop     %ebx
        pop     %edi
        ret
.size sigsafe_read, . - sigsafe_read
