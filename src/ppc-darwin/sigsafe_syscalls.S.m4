dnl Ignore the next line
/* This is automatically generated from sigsafe_syscalls.S.m4
/* $Id$ */

#import <architecture/ppc/asm_help.h>
#import <architecture/ppc/pseudo_inst.h>
#import <sys/syscall.h>

#define EINTR 4 /* from /usr/include/sys/errno.h */

.reference _sigsafe_key
NON_LAZY_STUB(_sigsafe_key)
NON_LAZY_STUB(_pthread_getspecific)


changequote([, ])

  define([forloop],
       [pushdef([$1], [$2])_forloop([$1], [$2], [$3], [$4])popdef([$1])])dnl
  define([_forloop],
       [$4[]ifelse($1, [$3], ,
            [define([$1], incr($1))_forloop([$1], [$2], [$3], [$4])])])dnl

define(SYSCALL, [
NESTED(_sigsafe_$1, 0, 1, 0, 0)
forloop([i], [1], [$2], [define([reg],[r[]eval(i+2)])
        stw     reg,ARG_IN(i)(r1)])

        PICIFY(_sigsafe_key)                ; retrieve TSD
        lwz     r3,0(PICIFY_REG)
        CALL_EXTERN_AGAIN(_pthread_getspecific)
        mr      r10,r3
forloop([i], [1], [$2], [define([reg],[r[]eval(i+2)])
        lwz     reg,ARG_IN(i)(r1)])

        li      r0,SYS_$1
        cmpwi   r10,0                        ; if TSD is NULL, don't dereference
        beq     _sigsafe_$1_maxjmp
LABEL(_sigsafe_$1_minjmp)
        lwz     r10,0(r10)                   ; if signal received, go to eintr
        cmpwi   r10,0
        bne     _sigsafe_$1_jmpto
LABEL(_sigsafe_$1_maxjmp)
        sc
        neg     r3,r3                        ; Executed only on error
        RETURN
LABEL(_sigsafe_$1_jmpto)
        li      r3,-EINTR
        RETURN

])

SYSCALL(read, 3)
SYSCALL(readv, 3)
SYSCALL(write, 3)
SYSCALL(writev, 3)
SYSCALL(select, 5)
SYSCALL(kevent, 6)
SYSCALL(wait4, 4)
