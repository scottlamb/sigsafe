/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#define _ASM
#include <sys/trap.h>
#include <sys/syscall.h>
#include <sys/errno.h>

.comm sigsafe_key, 4

/*
 * The tricky things to remember below are:
 *
 * - DELAY SLOTS!
 *   SPARC executes the next instruction in the pipeline before evaluating
 *   a control transfer instruction (jump, call). Thus, things can appear to
 *   happen out of sequence. ("Why is there an instruction after the return?")
 *
 * - ERESTART - we need to loop over the system call when it returns this
 *   error, of course with correct signal handling. Watch with a null TSD
 *   vs. having TSD contents.
 *
 * - "TE" - I enter the system call with a trap-if-equal to avoid using
 *   an extra no-op in the delay slot. The two places it will happen are after
 *   a BE when the TSD is null (thus TE is evaluated) or in a BNE's delay slot
 *   (thus, the TE is evaluated only if not taking the branch).
 *
 * There is one extraneous instruction here - the move. But to get rid of it,
 * I would have to put the restore between minjmp and maxjmp. That would make
 * the signal handler more complicated and different from all the other
 * platforms. Not worth it.
 */

#define SYSCALL(name,args) \
.text                                                               ;\
.global sigsafe_##name                                              ;\
.type sigsafe_##name,#function                                      ;\
sigsafe_##name:                                                     ;\
        save    %sp,-96,%sp                                         ;\
        set     sigsafe_key, %g1                                    ;\
        call    pthread_getspecific                                 ;\
        ld      [%g1], %o0                                          ;\
        mov     %o0, %g2                                            ;\
        restore                                                     ;\
        cmp     %g2, 0                                              ;\
$sigsafe_##name##_check:                                            ;\
        be      _sigsafe_##name##_maxjmp                            ;\
        mov     SYS_##name, %g1                                     ;\
LABEL(_sigsafe_##name##_minjmp)                                     ;\
        ld      [%g2], %g3                                          ;\
        cmp     %g3, 0                                              ;\
        bne     _sigsafe_##name##_jmpto                             ;\
.empty /* suppress "label in delay slot" warning */                 ;\
LABEL(_sigsafe_##name##_maxjmp)                                     ;\
        te      ST_SYSCALL                                          ;\
        bcc     $sigsafe_##name##_out                               ;\
        cmp     %o0, ERESTART                                       ;\
        be      $sigsafe_##name##_check                             ;\
        cmp     %g2, 0                                              ;\
        retl                                                        ;\
        neg     %o0                                                 ;\
LABEL(_sigsafe_##name##_jmpto)                                      ;\
        mov     -EINTR, %o0                                         ;\
$sigsafe_##name##_out:                                              ;\
        retl                                                        ;\
        nop                                                         ;\
.size sigsafe_##name, . - sigsafe_##name

#define LABEL(label) .global label; label:

#include "syscalls.h"
