/* $Id: read.s 583 2004-02-12 01:07:40Z slamb $ */

#import <architecture/ppc/asm_help.h>
#import <architecture/ppc/pseudo_inst.h>
#import <sys/syscall.h>

#ifndef __DYNAMIC__
#error not dynamic
#endif

#define EINTR 4 /* from /usr/include/sys/errno.h */
#define SIGSAFE_SYSCALLS_READ_IDX 0

; ssize_t sigsafe_read(int fd, void *buf, size_t count)
;                      r3      r4         r5
;                      (1w)    (1w)       (1w)

NESTED(_sigsafe_read, 0, 3, 0, 0)
        mr      r6,r3                       ; save a copy of fd; we'll overwrite r3
        EXTERN_TO_REG(r3,_sigsafe_key)      ; retrieve TSD
        CALL_EXTERN(_pthread_getspecific)
        cmpwi   r3,0                        ; if NULL, don't dereference
        beq     go
minjmp: lwz     r7,0(r3)                    ; if signal received, go to eintr
        cmpwi   r7,0
        bne     eintr
go:     mr      r3,r6                       ; put fd back into r3 for system call
        li      r0,SYS_read
maxjmp: sc
        b       error                       ; Error path
        RETURN
jmpto:  ; XXX set signal mask correctly
eintr:  li      r3,EINTR
error:  neg     r3,r3
        RETURN

/*
NESTED(_sigsafe_read_init, 0, 0, 0, 0)
        EXTERN_TO_REG(r3,_sigsafe_syscalls)
        li      r4,minjmp-_sigsafe_read     ; store minjmp offset
        stw     r4, 8(r3)
        li      r4,maxjmp-_sigsafe_read     ; store maxjmp offset
        stw     r4,12(r3)
        li      r4,jmpto-_sigsafe_read      ; store jmpto  offset
        stw     r4,16(r3)
        RETURN
 */
