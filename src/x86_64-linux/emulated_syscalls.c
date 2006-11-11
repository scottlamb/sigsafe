/** @file
 * Emulated system calls on x86_64-linux.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include "sigsafe_internal.h"

ssize_t
sigsafe_recv(int s, void *buf, size_t len, int flags)
{
    return sigsafe_recvfrom(s, buf, len, flags, NULL, NULL);
}

ssize_t
sigsafe_send(int s, const void *buf, size_t len, int flags)
{
    return sendto(s, buf, len, flags, NULL, 0);
}
