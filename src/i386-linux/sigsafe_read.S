#include <sys/syscall.h>

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

.text
.type sigsafe_read,@function
.global sigsafe_read
sigsafe_read:
        movl $__NR_read,%eax
        push %edi
        push %ebx
        movl %esp,%edi
        movl 0x0c(%edi),%ebx
        movl 0x10(%edi),%ecx
        movl 0x14(%edi),%edx
.global _sigsafe_read_maxjmp
_sigsafe_read_maxjmp:
        int $0x80
        pop %ebx
        pop %edi
        ret
.size sigsafe_read, . - sigsafe_read
