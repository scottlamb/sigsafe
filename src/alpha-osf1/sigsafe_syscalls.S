/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#include <sys/syscall.h>
#include <alpha/regdef.h>
#include <alpha/pal.h>

#define EINTR 4 /* from sys/errno.h */

/*
 * Function call convention:
 *
 * Register usage, from table 6-1 in the assembler guide:
 *
 *     register name    regdef.h name       usage
 *     $0               v0                  integer return values
 *     $1-$8            t0-t7               temporary
 *     $9-$14           s0-s5               saved registers
 *     $15/$fp          s6/fp               frame pointer
 *     $16-$21          a0-a5               integral arguments
 *     $22-$25          t8-t11              temporary
 *     $26              ra                  return address
 *     $27              pv/t12              procedure value / temporary
 *     $29/$gp          gp                  global pointer
 *     $30/$sp          sp                  stack pointer
 *     $31              zero                always zero
 *
 * See section 6.4 in the assembler guide for setting up the prologue
 */

/*
 * From alpha/asm.h:
 *
 * SYSCALL -- standard system call sequence
 * The kernel expects arguments to be passed with the normal C calling
 * sequence.  v0 should contain the system call number.  On return from
 * the kernel mode, a3 will be 0 to indicate no error and non-zero to
 * indicate an error; if an error occurred v0 will contain an errno.
 * if the kernel return an error, setup a valid gp and jmp to _cerror
 */

#ifdef _THREAD_SAFE
.comm sigsafe_key,4
#else
.comm sigsafe_data,24
#endif

#define SYSCALL(name, args)                                              \
.text                                                                   ;\
.globl sigsafe_##name                                                   ;\
.ent sigsafe_##name                                                     ;\
sigsafe_##name:                                                         ;\
        START                                                           ;\
        beq     v0, $sigsafe_##name##_nocompare                         ;\
LABEL(_sigsafe_##name##_minjmp)                                         ;\
        ldl     t0, 0(v0)                                               ;\
        bne     t0, _sigsafe_##name##_jmpto                             ;\
$sigsafe_##name##_nocompare:                                            ;\
        lda     v0, SYS_##name(zero)                                    ;\
LABEL(_sigsafe_##name##_maxjmp)                                         ;\
        call_pal PAL_callsys                                            ;\
        bne     a3, $sigsafe_##name##_error                             ;\
        lda     sp, 8+8*args(sp)                                        ;\
        ret     zero, (ra), 1                                           ;\
LABEL(_sigsafe_##name##_jmpto)                                          ;\
        lda     v0, EINTR(zero)                                         ;\
$sigsafe_##name##_error:                                                ;\
        negl    v0                                                      ;\
        lda     sp, 8+8*args(sp)                                        ;\
        ret     zero, (ra), 1                                           ;\
.end sigsafe_##name

#define LABEL(label) .globl label; label:

/**
 * SAVEDMASK_##args is the registers to save for an args##-argument system
 * call. We need to preserve the return address and each argument, since the
 * pthread_getspecific call will clobber them.
 */
/*@{*/
#define SAVEDMASK_0             (1<<26)                 /* save    $26 / ra */
#define SAVEDMASK_1             (SAVEDMASK_0 | (1<<16)) /* ... and $16 / a0 */
#define SAVEDMASK_2             (SAVEDMASK_1 | (1<<17)) /* ... and $17 / a1 */
#define SAVEDMASK_3             (SAVEDMASK_2 | (1<<18)) /* ... and $18 / a2 */
#define SAVEDMASK_4             (SAVEDMASK_3 | (1<<19)) /* ... and $19 / a3 */
#define SAVEDMASK_5             (SAVEDMASK_4 | (1<<20)) /* ... and $20 / a4 */
#define SAVEDMASK_6             (SAVEDMASK_5 | (1<<21)) /* ... and $21 / a5 */
/*@}*/

/**
 * Actually do the work of saving/restoring the arguments.
 */
/*@{*/
#define SAVERESTORE_REGS_0(op)                                           \
        op      ra,0(sp)
#define SAVERESTORE_REGS_1(op)                                           \
        SAVERESTORE_REGS_0(op)                                          ;\
        op      a0,8(sp)
#define SAVERESTORE_REGS_2(op)                                           \
        SAVERESTORE_REGS_1(op)                                          ;\
        op      a1,16(sp)
#define SAVERESTORE_REGS_3(op)                                           \
        SAVERESTORE_REGS_2(op)                                          ;\
        op      a2,24(sp)
#define SAVERESTORE_REGS_4(op)                                           \
        SAVERESTORE_REGS_3(op)                                          ;\
        op      a3,32(sp)
#define SAVERESTORE_REGS_5(op)                                           \
        SAVERESTORE_REGS_4(op)                                          ;\
        op      a4,40(sp)
#define SAVERESTORE_REGS_6(op)                                           \
        SAVERESTORE_REGS_5(op)                                          ;\
        op      a5,48(sp)
/*@}*/

#ifdef _THREAD_SAFE
#define START(args) \
        .frame  sp,8+8*args,ra                                          ;\
        .mask   SAVEDMASK_##args,-8-8*args                              ;\
        ldgp    gp,0(pv)                                                ;\
        lda     sp,-8-8*args(sp)                                        ;\
        SAVERESTORE_REGS_##args(stq)                                    ;\
        .prologue 1                                                     ;\
        lda     t0, sigsafe_key                                         ;\
        ldl     a0, 0(t0)                                               ;\
        jsr     ra,__pthread_getspecific                                ;\
        ldgp    gp,0(ra)                                                ;\
        SAVERESTORE_REGS_##args(ldq)
#else
#define START(args)
        .frame  sp,8,ra                                                 ;\
        .mask   SAVEDMASK_0,-8                                          ;\
        ldgp    gp,0(pv)                                                ;\
        lda     sp,-8(sp)                                               ;\
        .prologue 1                                                     ;\
        lda     t0, sigsafe_data                                        ;\
        ldl     v0, 0(t0)
#endif

#include "syscalls.h"
