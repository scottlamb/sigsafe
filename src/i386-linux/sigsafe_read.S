#include <sys/syscall.h>

.text
.type sigsafe_read,@function
.global sigsafe_read
sigsafe_read:
        movl $__NR_read,%eax
        push %edi
        push %esi
        push %ebx
        movl %esp,%edi
        movl 0x10(%edi),%ebx
        movl 0x14(%edi),%ecx
        movl 0x18(%edi),%edx
        movl 0x1c(%edi),%esi
        movl 0x20(%edi),%edi
.global _sigsafe_read_maxjmp
_sigsafe_read_maxjmp:
        int $0x80
        pop %ebx
        pop %esi
        pop %edi
        ret
.size sigsafe_read, . - sigsafe_read
