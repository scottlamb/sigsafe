ssize_t sigsafe_read(int fd, void *buf, size_t count) {
    struct sigsafe_tsd *tsd;
    int old_cancellation_type;

    /*
     * This is not async cancel-safe; it needs to be done first.
     * (Likewise, assigning to errno is probably not async cancel-safe.)
     */
    tsd = (struct sigsafe_tsd*) pthread_getspecific(sigsafe_key);

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old_cancellation_type);

    if (tsd != NULL && signal_jump) {
        /*
         * The instruction that checks signal_jump marks the beginning of the
         * signal_jump region.
         */
        pthread_setcanceltype(&old_cancellation_type, NULL);
        errno = EINTR;
        return -1;
    }

    /*
     * Setup and system call
     * The "sc" instruction marks the end of the signal_jump region.
     */

success:
    pthread_setcanceltype(&old_cancellation_type, NULL);
    errno = 0;
    return /*WHATEVER*/;

failure:
    pthread_setcanceltype(&old_cancellation_type, NULL);
    errno = /*WHATEVER*/;
    return -1;

signal_jump:
    pthread_setcanceltype(&old_cancellation_type, NULL);
    pthread_sigmask(SIG_SETMASK, &tsd->sigmask, NULL);
    errno = EINTR;
    return -1;
}
