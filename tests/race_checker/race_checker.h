/**
* @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is released under the MIT license.
 * @version         $Id$
 * @author          Scott Lamb &lt;slamb@slamb.org&gt;
 */

enum error_return_type {
    DIRECT,
    NEGATIVE,
    ERRNO
};

int error_wrap(int, const char*, enum error_return_type);

/**
 * @defgroup test_cancelpoints_io IO cancellation points
 */
//@{

void* create_pipe(void);
void cleanup_pipe(void*);

void do_safe_read(void*);
void do_unsafe_read(void*);
void nudge_read(void*);
//@}
