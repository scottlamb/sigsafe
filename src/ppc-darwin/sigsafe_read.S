/* $Id: read.s 583 2004-02-12 01:07:40Z slamb $ */

#import <architecture/ppc/asm_help.h>
#import <architecture/ppc/pseudo_inst.h>
#import <sys/syscall.h>

#define SIG_SETMASK 3 /* from /usr/include/sys/signal.h */
#define EINTR 4 /* from /usr/include/sys/errno.h */

; ssize_t sigsafe_read(int fd, void *buf, size_t count)
;                      r3      r4         r5
;                      (1w)    (1w)       (1w)

.reference _sigsafe_key

        .text
NESTED(_sigsafe_read, 8, 3, 0, 0)
        stw     r6,LOCAL_VAR(1)(r1)         ; make LOCAL_VAR(1) easy to find
                                            ; in debugger
        stw     r3,ARG_IN(1)(r1)
        stw     r4,ARG_IN(2)(r1)
        stw     r5,ARG_IN(3)(r1)
        EXTERN_TO_REG(r3,_sigsafe_key)      ; retrieve TSD
        CALL_EXTERN(_pthread_getspecific)
        cmpwi   r3,0                        ; if NULL, don't dereference
        beq     go
        stw     r3,LOCAL_VAR(2)(r1)
        addi    r3,r3,12
        CALL_EXTERN(__setjmp)
        cmpwi   r3,0
        bne     jumped
        lwz     r7,LOCAL_VAR(2)(r1)
        lwz     r3,ARG_IN(1)(r1)
        lwz     r4,ARG_IN(2)(r1)
        lwz     r5,ARG_IN(3)(r1)
LABEL(_sigsafe_read_minjmp)
        lwz     r8,0(r7)                    ; if signal received, go to eintr
        cmpwi   r8,0
        bne     eintr
go:     li      r0,SYS_read
LABEL(_sigsafe_read_maxjmp)
        sc
        neg     r3,r3                       ; Executed only on error
        RETURN
jumped: li      r3,SIG_SETMASK
        ; LOCAL_VAR(1) is now the sigset_t, courtesy of the signal handler
        lwz     r4,LOCAL_VAR(1)(r1)
        sub     r5,r5,r5
        CALL_EXTERN(_pthread_sigmask)
eintr:  li      r3,-EINTR
        RETURN
