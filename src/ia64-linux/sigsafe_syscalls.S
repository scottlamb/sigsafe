/*
 * $Id: sigsafe_syscalls.S 739 2004-05-07 10:13:47Z slamb $
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#define __ASSEMBLY__
#include <asm/unistd.h>
#include <asm/errno.h>

#define save_pfs    loc0
#define save_rp     loc1
#define save_gp     loc2

#define SYSCALL(name, args) \
.text                                                                   ;\
.global sigsafe_##name                                                  ;\
.proc sigsafe_##name                                                    ;\
sigsafe_##name:                                                         ;\
        .prologue                                                       ;\
        alloc save_pfs = ar.pfs, args, 3, OUTPUTS_##args, 0             ;\
        mov save_rp = rp                                                ;\
        .body                                                           ;\
        LOAD_TSD                                                        ;\
LABEL(sigsafe_##name##_minjmp_)                                         ;\
        cmp.ne.unc p6,p7 = 0,ret0 /* sigsafe_data_ == NULL */           ;\
        ;;                                                              ;\
(p6)    ld4 ret0 = [ret0]                                               ;\
        mov r15 = __NR_##name                                           ;\
        ;;                                                              ;\
(p6)    cmp.eq.unc p6,p7 = 0,ret0 /* sigsafe_data_->signal_received == 0 */ ;\
        COPY_INPUTS_TO_OUTPUTS_##args                                   ;\
        ;;                                                              ;\
LABEL(sigsafe_##name##_maxjmp_)                                         ;\
(p6)    break.i __BREAK_SYSCALL                                         ;\
(p7)    br.cond.spnt.few sigsafe_##name##_jmpto_                        ;\
        ;;                                                              ;\
        cmp.eq.unc p6,p0 = -1,r10           /* if error ... */          ;\
        ;;                                                              ;\
(p6)    sub ret0 = 0,ret0                   /* ... return -errno */     ;\
        mov rp = save_rp                                                ;\
        mov ar.pfs = save_pfs                                           ;\
        br.ret.sptk.few rp                                              ;\
        ;;                                                              ;\
LABEL(sigsafe_##name##_jmpto_)                                          ;\
        mov ret0 = -EINTR                                               ;\
        mov rp = save_rp                                                ;\
        mov ar.pfs = save_pfs                                           ;\
        br.ret.sptk.few rp                                              ;\
.endp sigsafe_##name

#define LABEL(label)                                                     \
.global label                                                           ;\
label:

/**
 * @define LOAD_TSD
 * Loads the TSD pointer into ret0
 */
#ifdef _THREAD_SAFE
#define LOAD_TSD \
        addl out0 = @ltoff(sigsafe_key_), gp                            ;\
        mov save_gp = gp                                                ;\
        ;;                                                              ;\
        ld8 out0 = [out0] /* &sigsafe_key_ */                           ;\
        ;;                                                              ;\
        ld4 out0 = [out0] /* sigsafe_key_  */                           ;\
        ;;                                                              ;\
        br.call.sptk.few rp = pthread_getspecific#                      ;\
        ;;                                                              ;\
        mov gp = save_gp
#else
#define LOAD_TSD \
        addl ret0 = @ltoff(sigsafe_data_), gp                           ;\
        ;;                                                              ;\
        ld8 ret0 = [ret0] /* &sigsafe_data_ */                          ;\
        ;;                                                              ;\
        ld8 ret0 = [ret0] /* sigsafe_data_ */                           ;\
        ;;
#endif

#define COPY_INPUTS_TO_OUTPUTS_0
#define COPY_INPUTS_TO_OUTPUTS_1 \
        mov out0 = in0
#define COPY_INPUTS_TO_OUTPUTS_2 \
        COPY_INPUTS_TO_OUTPUTS_1; \
        mov out1 = in1
#define COPY_INPUTS_TO_OUTPUTS_3 \
        COPY_INPUTS_TO_OUTPUTS_2; \
        mov out2 = in2
#define COPY_INPUTS_TO_OUTPUTS_4 \
        COPY_INPUTS_TO_OUTPUTS_3; \
        mov out3 = in3
#define COPY_INPUTS_TO_OUTPUTS_5 \
        COPY_INPUTS_TO_OUTPUTS_4; \
        mov out4 = in4
#define COPY_INPUTS_TO_OUTPUTS_6 \
        COPY_INPUTS_TO_OUTPUTS_5; \
        mov out5 = in5

#ifdef _THREAD_SAFE
#define OUTPUTS_0 1 /* to pthread_getspecific */
#else
#define OUTPUTS_0 0
#endif
#define OUTPUTS_1 1
#define OUTPUTS_2 2
#define OUTPUTS_3 3
#define OUTPUTS_4 4
#define OUTPUTS_5 5
#define OUTPUTS_6 6


#include "syscalls.h"
