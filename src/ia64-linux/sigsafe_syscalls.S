/*
 * $Id: sigsafe_syscalls.S 739 2004-05-07 10:13:47Z slamb $
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#define __ASSEMBLY__
#include <asm/unistd.h>
#include <asm/errno.h>

#ifdef _THREAD_SAFE
#define outputs 1 /* to pthread_getspecific */
#else
#define outputs 0
#endif

/* for 3-argument form anyway */
/* loc0..locN doesn't seem to be working */
#define old_pfs     r35
#define ra          r36
#define signaled    r37

#define SYSCALL(name, args) \
.text                                                                   ;\
.global sigsafe_##name                                                  ;\
.proc sigsafe_##name                                                    ;\
sigsafe_##name:                                                         ;\
        alloc old_pfs = ar.pfs, args, args, 3, 0                        ;\
        mov ra = b0                                                     ;\
        LOAD_TSD                                                        ;\
        ;;                                                              ;\
LABEL(sigsafe_##name##_minjmp_)                                         ;\
        cmp.ne.unc p6,p7 = 0,ret0                                       ;\
        ;;                                                              ;\
(p6)    ld4 signaled = [ret0]                                           ;\
(p6)    mov r15 = __NR_##name                                           ;\
        ;;                                                              ;\
(p6)    cmp.eq.unc p6,p7 = 0,signaled                                   ;\
        ;;                                                              ;\
LABEL(sigsafe_##name##_maxjmp_)                                         ;\
(p6)    break.i __BREAK_SYSCALL                                         ;\
(p7)    br.cond.spnt.few sigsafe_##name##_jmpto_#                       ;\
        ;;                                                              ;\
        cmp.eq.unc p6,p0 = -1,r10           /* if error ... */          ;\
        ;;                                                              ;\
(p6)    sub ret0 = 0,ret0                   /* ... return -errno */     ;\
        br.ret.sptk.few rp                                              ;\
        ;;                                                              ;\
LABEL(sigsafe_##name##_jmpto_)                                          ;\
        mov ret0 = -EINTR                                               ;\
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
        addl o0 = @ltoff(sigsafe_key_#), gp                             ;\
        ;;                                                              ;\
        ld4 o0 = [o0]                                                   ;\
        ;;                                                              ;\
        br.call.sptk.few b0 = pthread_getspecific#
#else
#define LOAD_TSD \
        addl ret0 = @ltoff(sigsafe_data_#), gp
#endif

#include "syscalls.h"
