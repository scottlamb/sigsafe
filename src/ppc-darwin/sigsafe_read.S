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
NESTED(_sigsafe_read, 0, 1, 0, 0)
        stw     r3,ARG_IN(1)(r1)
        stw     r4,ARG_IN(2)(r1)
        stw     r5,ARG_IN(3)(r1)
        EXTERN_TO_REG(r3,_sigsafe_key)       ; retrieve TSD
        CALL_EXTERN(_pthread_getspecific)
        mr      r10,r3
        lwz     r3,ARG_IN(1)(r1)
        lwz     r4,ARG_IN(2)(r1)
        lwz     r5,ARG_IN(3)(r1)
        li      r0,SYS_read
        cmpwi   r10,0                        ; if TSD is NULL, don't dereference
        beq     _sigsafe_read_maxjmp
LABEL(_sigsafe_read_minjmp)
        lwz     r10,0(r10)                   ; if signal received, go to eintr
        cmpwi   r10,0
        bne     _sigsafe_read_jmpto
LABEL(_sigsafe_read_maxjmp)
        sc
        neg     r3,r3                        ; Executed only on error
        RETURN
LABEL(_sigsafe_read_jmpto)
        li      r3,-EINTR
        RETURN
