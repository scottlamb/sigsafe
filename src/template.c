/** @file 
 * A template for making assembly.
 * This file should never be used. Instead, it's helpful as a template
 * for writing assembly for new platforms. Compile it with -S and look at the
 * resultant assembly code. Try with and without -D_THREAD_SAFE and -fpic.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <errno.h>
#include <pthread.h>
#include "sigsafe_internal.h"

#ifdef _THREAD_SAFE
PRIVATE_DEF(pthread_key_t sigsafe_key_);
#else
PRIVATE_DEF(struct sigsafe_tsd_ *sigsafe_data_);
#endif

ssize_t sigsafe_read_template(int fd, char *buf, size_t len) {
#ifdef _THREAD_SAFE
    struct sigsafe_tsd_ *sigsafe_data_ = pthread_getspecific(sigsafe_key_);
#endif
    if (sigsafe_data_ == NULL || sigsafe_data_->signal_received == 0) {
        return read(fd, buf, len);
    }
    return -EINTR;
}
