#include <sys/syscall.h>

#define simple_kernel_trap(trap_name, trap_number)      \
        .globl  _##trap_name                            @\
_##trap_name:                                           @\
        li      r0,trap_number                           @\
        sc                                               @\
        blr                                              @\
        END(trap_name)

#define kernel_trap_0(trap_name,trap_number)             \
        simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_1(trap_name,trap_number)             \
        simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_2(trap_name,trap_number)             \
        simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_3(trap_name,trap_number)             \
        simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_4(trap_name,trap_number)             \
        simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_5(trap_name,trap_number)             \
        simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_6(trap_name,trap_number)             \
        simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_7(trap_name,trap_number)             \
        simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_8(trap_name,trap_number)             \
        simple_kernel_trap(trap_name,trap_number)

#define kernel_trap_9(trap_name,trap_number)             \
        simple_kernel_trap(trap_name,trap_number)

#define SYSCALL(name, nargs)                    \
        .globl  cerror                          @\
LEAF(_##name)                                   @\
        kernel_trap_args_##nargs                @\
        li      r0,SYS_##name                   @\
        sc                                      @\
        b       1f                              @\
        blr                                     @\
1:      BRANCH_EXTERN(cerror)                   @\
.text                                           \
2:      nop
