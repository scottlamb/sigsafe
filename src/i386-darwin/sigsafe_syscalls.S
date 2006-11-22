/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#ifdef __DYNAMIC__
/*
 * XXX: Just a warning because I don't know why __DYNAMIC__ is defined.
 * I'm only building a static library.
 */
#warn "This code is position-dependent."
#endif

#include <sys/syscall.h>
#include <sigsafe_config.h>
#include <architecture/i386/asm_help.h>

#define EINTR 4 /* from sys/errno.h> */
#define KERN_ABORTED 14 /* from /usr/include/mach/kern_return.h */

/* system call number in %eax */

#define GENERIC_SYSCALL(name,type,args)                                  \
.text                                                                   ;\
/*XXX: .type sigsafe_##name,@function*/                                 ;\
LABEL(_sigsafe_##name)                                                  ;\
        LOAD_TSD                                                        ;\
        testl   %eax,%eax                                               ;\
        je      L_sigsafe_##name##_nocompare /*WTF?*/                   ;\
HIDDEN(_sigsafe_##name##_minjmp_)                                       ;\
        cmp     $0,(%eax)                                               ;\
        jne     _sigsafe_##name##_jmpto_                                ;\
L_sigsafe_##name##_nocompare:                                           ;\
        movl    $SYS_##name,%eax                                        ;\
        SYSCALL_BOTTOM_##type(name)                                     ;\
L_sigsafe_##name##_out:                                                 ;\
        ret                                                             ;\
/*XXX: .size sigsafe_##name, . - sigsafe_##name */

#define SYSCALL_BOTTOM_mach(name)                                        \
HIDDEN(_sigsafe_##name##_maxjmp_)                                       ;\
        int     $0x81                                                   ;\
        ret                                                             ;\
HIDDEN(_sigsafe_##name##_jmpto_)                                        ;\
        movl    $KERN_ABORTED,%eax

#define SYSCALL_BOTTOM_posix(name)                                       \
        /* need to put return address in %edx, stack in %ecx */         ;\
        call L_sigsafe_##name##_call                                    ;\
L_sigsafe_##name##_call:                                                ;\
        pop     %edx /* contains L_sigsafe_##name##_call */             ;\
        add     $(L_sigsafe_##name##_back-L_sigsafe_##name##_call),%edx ;\
        movl    %esp,%ecx                                               ;\
HIDDEN(_sigsafe_##name##_maxjmp_)                                       ;\
        sysenter                                                        ;\
L_sigsafe_##name##_back:                                                ;\
        jae     L_sigsafe_##name##_out                                  ;\
        neg     %eax /* error path */                                   ;\
        ret                                                             ;\
HIDDEN(_sigsafe_##name##_jmpto_)                                        ;\
        movl    $-EINTR,%eax

#define HIDDEN(label) \
.private_extern label                                                   ;\
        label:

#ifdef _THREAD_SAFE
/* stole this stub from gcc output */
        .section __IMPORT,__jump_table,symbol_stubs,self_modifying_code+pure_instructions,5
L_pthread_getspecific$stub:
        .indirect_symbol _pthread_getspecific
        hlt ; hlt ; hlt ; hlt ; hlt
        .subsections_via_symbols

#define LOAD_TSD \
        pushl   _sigsafe_key_                                           ;\
        call    L_pthread_getspecific$stub                              ;\
        pop     %ecx /* discarded */
#else
#define LOAD_TSD \
        mov     _sigsafe_data_,%eax
#endif

#define SYSCALL(name,args)      GENERIC_SYSCALL(name,posix,args)
#define MACH_SYSCALL(name,args) GENERIC_SYSCALL(name,mach,args)

#include "syscalls.h"
