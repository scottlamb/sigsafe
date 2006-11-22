/*
 * $Id$
 * Copyright (C) 2004 Scott Lamb <slamb@slamb.org>
 * This file is part of sigsafe, which is released under the MIT license.
 */

#ifdef __DYNAMIC__
#error "This code is position-dependent."
#endif

#include <sys/syscall.h>
#include <sigsafe_config.h>
#include <architecture/i386/asm_help.h>

#define EINTR 4 /* from sys/errno.h> */

/* system call number in %eax */

#define SYSCALL(name,args)                                               \
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
        movl    $-EINTR,%eax                                            ;\
L_sigsafe_##name##_out:                                                 ;\
        ret                                                             ;\
/*XXX: .size sigsafe_##name, . - sigsafe_##name */

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

#include "syscalls.h"
