/* $Id$ */

void sighandler_platform(ucontext_t *ctx) {
    if (/* thread in jump region */) {
        int *old_cancellation_type = /* place up stack */;
        sigset_t *uc_sigmask = /* place up stack */;

        pthread_setcanceltype(*old_cancellation_type, NULL);
        memcpy(uc_sigmask, ctx->uc_sigmask, sizeof(sigset_t));

        /*
         * adjust pointer and jump
         */
    }
}
