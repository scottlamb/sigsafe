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
define(SYSCALL, [
NESTED(_sigsafe_$1, 0, 1, 0, 0)
        stw     r3,ARG_IN(1)(r1)
        stw     r4,ARG_IN(2)(r1)
        stw     r5,ARG_IN(3)(r1)
        PICIFY(_sigsafe_key)                ; retrieve TSD
        lwz     r3,0(PICIFY_REG)
        CALL_EXTERN_AGAIN(_pthread_getspecific)
        mr      r10,r3
        lwz     r3,ARG_IN(1)(r1)
        lwz     r4,ARG_IN(2)(r1)
        lwz     r5,ARG_IN(3)(r1)
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
