/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#include <asm/unistd.h>
#include <asm/errno.h>
#include <alpha/regdef.h>

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
 *
 * Linux system call convention:
 * v0 should hold __NR_#name on start
 * a0-a5 hold the arguments (just as they were passed in)
 * use callsys instruction
 * v0 on exit holds the return value; errors positive
 * a3 on exit is zero on success
 */

.comm sigsafe_key,4

changequote([, ])

define(SYSCALL, [
/* Assuming three arguments for now */
.text
.type sigsafe_$1,@function
.ent sigsafe_$1
.globl sigsafe_$1
sigsafe_$1:
        .frame  sp,8+8*$2,ra
        .mask   0x4070000,-8-8*$2 /* save a0-a2 and ra */
        ldgp    gp,0(pv)
        lda     sp,-8-8*$2(sp)
        stq	ra,0(sp)
        stq	a0,8(sp)
        stq	a1,16(sp)
        stq	a2,24(sp)
        .prologue 1

        /* Retrieve TSD with call to pthread_getspecific. */
        lda     t0, sigsafe_key
        ldl     a0, 0(t0)
        jsr     ra,pthread_getspecific
        ldgp    gp,0(ra)
        ldq     ra,0(sp)
        /* now v0 holds the TSD address */

        /* Restore arguments, clobbered by call. */
        ldq     a0, 8(sp)
        ldq     a1, 16(sp)
        ldq     a2, 24(sp)

        /* Skip to _sigsafe_$1_nocompare if TSD pointer is NULL */
        beq     v0, $sigsafe_$1_nocompare
.global _sigsafe_$1_minjmp
_sigsafe_$1_minjmp:
        /* Jump to _sigsafe_$1_jmpto if dereferenced value is not 0 */
        ldl     t0, 0(v0)
        bne     t0, _sigsafe_$1_jmpto
$sigsafe_$1_nocompare:
        lda     v0, __NR_$1(zero)
.global _sigsafe_$1_maxjmp
_sigsafe_$1_maxjmp:
        callsys
        bne     a3, $sigsafe_$1_error
        lda     sp, 8+8*$2(sp)
        ret     zero, (ra), 1
.global _sigsafe_$1_jmpto
_sigsafe_$1_jmpto:
        lda     v0, EINTR(zero)
$sigsafe_$1_error:
        negl    v0
        lda     sp, 8+8*$2(sp)
        ret     zero, (ra), 1
.end sigsafe_$1
])

include([syscalls.h])
