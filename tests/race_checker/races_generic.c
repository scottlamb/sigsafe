/**
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is released under the MIT license.
 * @version         $Id$
 * @author          Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <sigsafe.h>
#include "race_checker.h"

volatile sig_atomic_t signal_received;

static void note_signal(int signo) { signal_received++; }

void install_safe(void *test_data) {
    error_wrap(sigsafe_install_handler(SIGUSR1, NULL), "sigsafe_install_handler", ERRNO);
    error_wrap(sigsafe_install_tsd(NULL, NULL), "sigsafe_install_tsd", ERRNO);
}

void install_unsafe(void *test_data) {
    struct sigaction sa;
    sa.sa_handler = &note_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    error_wrap(sigaction(SIGUSR1, &sa, NULL), "sigaction", ERRNO);
}
