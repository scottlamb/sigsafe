/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>.
 * This file is part of sigsafe, which is released under the MIT license.
 */

#import <architecture/ppc/asm_help.h>
#import <architecture/ppc/pseudo_inst.h>
#import <sys/syscall.h>

#define EINTR 4 /* from /usr/include/sys/errno.h */

#ifdef _THREAD_SAFE
.reference _sigsafe_key_
NON_LAZY_STUB(_sigsafe_key_)
NON_LAZY_STUB(_pthread_getspecific)
#else
.reference _sigsafe_data_
NON_LAZY_STUB(_sigsafe_data_)
#endif

#define SYSCALL(name,args)                                                   \
NESTED(_sigsafe_##name, 0, 1, 0, 0)                                         @\
        LOAD_TSD(args)                                                      @\
        li      r0,SYS_##name                                               @\
        cmpwi   r10,0                        /* check TSD pointer */        @\
        beq     _sigsafe_##name##_maxjmp_                                   @\
HIDDEN(_sigsafe_##name##_minjmp_)                                           @\
        lwz     r10,0(r10)                   /* check TSD value */          @\
        cmpwi   r10,0                                                       @\
        bne     _sigsafe_##name##_jmpto_                                    @\
HIDDEN(_sigsafe_##name##_maxjmp_)                                           @\
        sc                                                                  @\
        neg     r3,r3                        /* executed only on error */   @\
        RETURN                                                              @\
HIDDEN(_sigsafe_##name##_jmpto_)                                            @\
        li      r3,-EINTR                                                   @\
        RETURN

#define HIDDEN(label) .private_extern label @ label:

#ifdef _THREAD_SAFE
#define LOAD_TSD(args)                                                       \
        SAVERESTORE_ARGS_##args(stw)                                        @\
        PICIFY(_sigsafe_key_)                /* retrieve TSD */             @\
        lwz     r3,0(PICIFY_REG)                                            @\
        CALL_EXTERN_AGAIN(_pthread_getspecific)                             @\
        mr      r10,r3                                                      @\
        SAVERESTORE_ARGS_##args(lwz)
#else
#define LOAD_TSD(args)                                                       \
        PICIFY(_sigsafe_data_)                                              @\
        lwz     r10,0(PICIFY_REG)
#endif

/**
 * Saves and restores our arguments before/after the call to
 * pthread_getspecific, which may clobber them.
 */
/*@{*/
#define SAVERESTORE_ARGS_0(func)
#define SAVERESTORE_ARGS_1(func)                                            @\
        func    r3, ARG_IN(1)(r1)
#define SAVERESTORE_ARGS_2(func)                                            @\
        SAVERESTORE_ARGS_1(func)                                            @\
        func    r4, ARG_IN(2)(r1)
#define SAVERESTORE_ARGS_3(func)                                            @\
        SAVERESTORE_ARGS_2(func)                                            @\
        func    r5, ARG_IN(3)(r1)
#define SAVERESTORE_ARGS_4(func)                                            @\
        SAVERESTORE_ARGS_3(func)                                            @\
        func    r6, ARG_IN(4)(r1)
#define SAVERESTORE_ARGS_5(func)                                            @\
        func    r7, ARG_IN(5)(r1)
#define SAVERESTORE_ARGS_6(func)                                            @\
        func    r8, ARG_IN(6)(r1)
/*@}*/

#include "syscalls.h"
