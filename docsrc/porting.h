// $Id$

/**
 * @page porting Porting to a new system
 * <h2>Writing the system-dependent code</h2>
 *
 * <h3>What needs to be written</h3>
 *
 * To port sigsafe to a new system, you need to implement:
 *
 * - The system call wrappers themselves. They're mostly normal wrappers
 *   except that they get a TSD (thread-specific data) key and look at
 *   received_signals within it. They should have a symbol _sigsafe_XXX_minjmp
 *   where they read the received_signals value from memory and
 *   _sigsafe_XXX_maxjmp where they execute the system call. Between those two
 *   values (inclusive), the signal handler will work by jumping to
 *   _sigsafe_XXX_jmpto. (It should not be in that region.) Look at the other
 *   platforms for examples.
 * 
 * - the signal handler. It should adjust the instruction pointer inside the
 *   context argument as mentioned above. Then return to userspace. On all the
 *   previous platforms, just modifying the context and returning normally is
 *   sufficient. You might find instead:
 * 
 *   - you need to use setcontext()
 *   - you need to use sigreturn()
 *   - you need to save a jump buffer in your system call wrappers with
 *     sigsetjmp() and restore it with siglongjmp() (ugh)
 *
 * And you can optionally implement:
 *
 * - the race condition tester. It uses process tracing, which is OS-specific.
 *   I had originally hoped this would simply be a matter of finding the right
 *   names for <tt>ptrace()</tt> constants. Unfortunately, it seems to be much
 *   more involved. In fact, this code is only working under Linux now.
 *   Luckily, it's not strictly necessary - you can test sigsafe without it,
 *   as outlined below.
 *
 * <h3>Finding resources</h3>
 *
 * I've had good luck so far finding information about how to implement raw
 * system calls under various platforms. You might find the following
 * resources helpful:
 *
 * - Google. Just search for your platform name and syscall. Include bits
 *   you find in system headers.
 * - The <tt>arch</tt>, <tt>machine</tt>, <tt>sys</tt>, etc. directories of
 *   off <tt>/usr/include</tt>. Searching for "syscall", "system call", "asm",
 *   and "assembler" will be helpful.
 * - Compiler-generated assembly. "Try cc -S file.c" after making a skeleton
 *   file that does most of what you want. You might try with or without
 *   optimization ("-O"); sometimes it's clearer with or without. You might
 *   also use "-fpic" under gcc to look at position-independent code.
 * - The system libc. If you're lucky, it's open source. But failing that,
 *   you can at least disassemble a system call with <tt>dbx</tt> or
 *   <tt>gdb</tt>.
 * - Existing sigsafe platforms. It will be especially helpful to look at
 *   implementations for the same architecture, if they exist. Function call
 *   conventions tend to be the same across operating systems, and system call
 *   conventions tend to be similar. Each platform also has a <tt>README</tt>
 *   file outlining the resources I used to make the port.
 * - Me. I'm happy to share whatever I know.
 *
 * <h3>Step-by-step tutorial</h3>
 *
 * ...doesn't exist yet. Sorry! Come back in a while.
 *
 * <h2>Testing your implementation</h2>
 *
 * <h3>Using the race condition checker</h3>
 *
 * Ideally, the race condition checker works on your platform. Then testing is
 * fairly simple:
 *
@verbatim
$ tests/build-myplatform/race_checker -qm
...
  Test                 Result               Expected            
----------------------------------------------------------------
  sigsafe_read         success              success             
  racebefore_read      ignored signal       ignored signal      
  raceafter_read       forgotten result     forgotten result    
@endverbatim
 *
 * If the the results are different from expected, it will mark the guilty
 * tests with a '*' and note it at the bottom.
 *
 * If the quick tests pass, you can run a full test with:
 *
@verbatim
$ ./race_checker -a
@endverbatim
 *
 * but you might go out for coffee or perhaps dinner while this happens. It
 * traces through a lot of instructions one-by-one, so it is slow.
 *
 * <h3>Testing for races with gdb</h3>
 *
 * <ul>
 *
 * <li> Test a run with no signals received:
 *
@verbatim
$ gdb build-myplatform/tests/simple_test
...
(gdb) run
Starting program: ...
asdf
read 5 bytes: asdf
@endverbatim
 *
 *   This is the most basic confirmation that your program is working, with no
 *   unusual paths taken.</li>
 *
 * <li> Test a run with a non-signal-related error. I typically use
 *      <tt>EBADF</tt> by modifying <tt>simple_test</tt>'s call line to read a
 *      nonsensical file descriptor.</li>
 *
 * <li>Test runs without a call to sigsafe_install_tsd. I typically comment
 *     this line out of <tt>simple_test</tt>. It shouldn't crash. The portable
 *     code won't call <tt>sigsafe_handler_for_platform_</tt> in this case, so
 *     you can be confident that it won't skip out on a signal handler. You
 *     just have to check that you don't try to dereference NULL; you should
 *     jump directly to the system call if you find the TSD pointer is null.
 *     </li>
 *
 * <li>Test a run with a signal received well before minjmp (but after the
 *     signal handler and TSD are installed).
 *
@verbatim
$ gdb build-myplatform/tests/simple_test
...
(gdb) break sigsafe_read
Breakpoint 1 at 0x22d0
(gdb) run
Starting program: ...

Breakpoint 1, 0x000022d0 in sigsafe_read ()
(gdb) signal SIGUSR1
Continuing with signal SIGUSR1.
[S]
Breakpoint 1, 0x000022d0 in sigsafe_read ()
(gdb) continue
sigsafe_read returned -4 (Interrupted system call)
@endverbatim
 *
 *     Note the "[S]" that says the signal handler was invoked. If you don't
 *     see this, your gdb might be broken! I've found under Darwin/ppc that I
 *     have to manually deliver a signal from a shell in another terminal
 *     window.
 *
 *     What you're looking for here is confirmation that it correctly follows
 *     the signal received path if a signal is received before entering the
 *     jump region.</li>
 *
 * <li>Test runs one instruction before minjmp. They should be the same as
 *     above. Use "stepinstruction" (or "si" for short) to step one-by-one
 *     until you almost see the minjmp, then try the signal. You're looking
 *     for confirmation that you didn't place the <tt>minjmp</tt> too late. If
 *     you've already fetched the <tt>signal_received</tt> count when
 *     <tt>minjmp</tt> comes around, this will proceed to the system call.
 *     That's bad.</li>
 *
 * <li>Test runs at minjmp, maxjmp, and possibly between. They should look the
 *     same as above, except you'll also see a <tt>[J]</tt> if debugging is
 *     enabled.</li>
 *
 * <li>Test runs immediately after the system call. sigsafe_read should return
 *     the normal result, not -EINTR.</li>
 * </ul>
 *
 * This is not as thorough as the automated race checker, but these are all
 * the critical values. You can be reasonable confident in your implementation
 * if all these tests pass.
 *
 * Some handy gdb commands:
 *
 * - disassemble (or "disas"): shows machine instructions at the current
 *   instruction pointer address or the specified function/symbol. Handy for
 *   seeing where you are in the code, and also for reverse-engineering
 *   <tt>libc</tt> where necessary.
 * - stepinstruction (or "si"): steps a single machine instruction.
 * - "show registers" - shows all registers. "show registers X Y Z" will only
 *   show the given ones.
 * - "set $x = 1", where x is a register, will set the value appropriately.
 * - "print expr" will print the value of the specified expression. It can
 *   include registers ($reg as above), C-style casts and operators (the '&'
 *   address-of or '*' derefence), etc.
 * - "signal SIGxxx" will deliver the specified signal and continue. Be
 *   careful - it doesn't work on all platforms. On Darwin/ppc, I have to
 *   manually deliver signals to the stopped program in another terminal, then
 *   continue.
 * - "break func" - set a breakpoint at a function
 * - "break *func+X" - sets a breakpoint at X bytes past the start of func.
 *   Handy when looking at disassembler output.
 *
 * <h3>Testing for races with dbx</h3>
 *
 * The process is as above, with gdb. The only real difference is in the
 * syntax. Some handy commands here:
 *
 * - "func/20i" - disassemble 20 instructions starting at func.
 * - "stop in func" - set a breakpoint at func.
 * - "continue SIGxxx" - continue with a signal.
 * - "printregs" - print register contents.
 *
 * <h3>Testing performance</h3>
 *
 * You should also run <tt>time build-myplatform/tests/bench_read_raw</tt>
 * and <tt>time build-myplatform/tests/bench_read_safe</tt>. They should not
 * differ in time significantly. (User time is where you'll find the
 * difference, if any.) In theory the safe version should be slightly more
 * processor-intensive since it makes a call to <tt>pthread_getspecific</tt>
 * with every system call. In practice, I often find no statistically
 * significant difference or even that the safe version is faster.
 */
