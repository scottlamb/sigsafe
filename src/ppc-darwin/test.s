/*
 * $Id$
 * Compile with: gcc test.s -o test
 */

.globl _main
.text
_main:
xor     r3, r3,r3       ;  r3 = 0
li      r0, 23          ;  syscall for setuid = 23
sc                      ;  system call setuid(r3)
li      r3, 1           ;  code execution resumes here
                        ;  if the setuid system call failed.
                        ;  r3 = 1 IF setuid(0) FAILED

li      r0, 1           ;  code execution resumes here
                        ;  if setuid(0) was a success.
                        ;  syscall for exit = 1sc
sc                      ;  system call exit(r3)
