/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>.
 * This file is part of sigsafe, which is released under the MIT license.
 */

#import <architecture/ppc/asm_help.h>
#import <architecture/ppc/pseudo_inst.h>
#import <sys/syscall.h>
#import <sigsafe_config.h>

#define EINTR 4 /* from /usr/include/sys/errno.h */
#define KERN_ABORTED 14 /* from /usr/include/sys/mach/kern_return.h */

#ifdef _THREAD_SAFE
.reference _sigsafe_key_
NON_LAZY_STUB(_sigsafe_key_)
NON_LAZY_STUB(_pthread_getspecific)
#else
.reference _sigsafe_data_
NON_LAZY_STUB(_sigsafe_data_)
#endif

#define ERROR_posix -EINTR
#define ERROR_mach  KERN_ABORTED

#define GENERIC_SYSCALL(name,type,args) \
LABEL(_sigsafe_##name)                                                      @\
        LOAD_TSD(args)                                                      @\
        cmpwi   r10,0                        /* check TSD pointer */        @\
        beq     _sigsafe_##name##_maxjmp_                                   @\
        li      r0,SYS_##name                                               @\
HIDDEN(_sigsafe_##name##_minjmp_)                                           @\
        lwz     r10,0(r10)                   /* check TSD value */          @\
        cmpwi   r10,0                                                       @\
        bne     _sigsafe_##name##_jmpto_                                    @\
HIDDEN(_sigsafe_##name##_maxjmp_)                                           @\
        sc                                                                  @\
        POST_SC_##type                                                      @\
        blr                                                                 @\
HIDDEN(_sigsafe_##name##_jmpto_)                                            @\
        li      r3,ERROR_##type                                             @\
        blr

/* For Mach system calls, we don't need to do anything post-system call. */
#define POST_SC_mach

/*
 * For POSIX system calls, it's a little different:
 * On error, we return to the next instruction. (This one.)
 * It should negate the return value so we know it was an error.
 * On success, the system call skips over this one.
 */
#define POST_SC_posix neg r3,r3

#define HIDDEN(label) .private_extern label @ label:

#ifdef _THREAD_SAFE
#define LOAD_TSD(args) \
BUILD_FRAME(0, 1, 0, 0)                                                     @\
        SAVERESTORE_ARGS_##args(stw)                                        @\
        PICIFY(_sigsafe_key_)                /* retrieve TSD */             @\
        lwz     r3,0(PICIFY_REG)                                            @\
        CALL_EXTERN_AGAIN(_pthread_getspecific)                             @\
        mr      r10,r3                                                      @\
        SAVERESTORE_ARGS_##args(lwz)                                        @\
        /* Stuff copied from RETURN { */                                     \
        lwz32   r0,r1,SAVED_LR                                              @\
        lwz32   r12,r1,SAVED_CR                                             @\
        addic   sp,r1,__framesize                                           @\
        mtlr    r0                                                          @\
        mtcrf   0xff,r12                                                     \
        /* } */
#else
#define LOAD_TSD(args) \
        PICIFY(_sigsafe_data_)                                              @\
        lwz     r10,0(PICIFY_REG)
#endif

/**
 * Saves and restores our arguments before/after the call to
 * pthread_getspecific, which may clobber them.
 */
/*@{*/
#define SAVERESTORE_ARGS_0(func)
#define SAVERESTORE_ARGS_1(func) \
        func    a0, ARG_IN(1)(sp)
#define SAVERESTORE_ARGS_2(func) \
        SAVERESTORE_ARGS_1(func)                                            @\
        func    a1, ARG_IN(2)(sp)
#define SAVERESTORE_ARGS_3(func) \
        SAVERESTORE_ARGS_2(func)                                            @\
        func    a2, ARG_IN(3)(sp)
#define SAVERESTORE_ARGS_4(func) \
        SAVERESTORE_ARGS_3(func)                                            @\
        func    a3, ARG_IN(4)(sp)
#define SAVERESTORE_ARGS_5(func) \
        SAVERESTORE_ARGS_4(func)                                            @\
        func    a4, ARG_IN(5)(sp)
#define SAVERESTORE_ARGS_6(func) \
        SAVERESTORE_ARGS_5(func)                                            @\
        func    a5, ARG_IN(6)(sp)
/*@}*/

#define SYSCALL(name,args)      GENERIC_SYSCALL(name,posix,args)
#define MACH_SYSCALL(name,args) GENERIC_SYSCALL(name,mach,args)

.private_extern _sigsafe_clock_sleep_trap
#include "syscalls.h"
