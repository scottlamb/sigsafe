#include <stdio.h>
#include <sigsafe.h>

int main(void) {
    char buf[42];
    int retval;

    sigsafe_install_handler(SIGUSR1, NULL);
    sigsafe_install_tsd(NULL, NULL);
    raise(SIGUSR1);
    memset(buf, 0, sizeof buf);
    retval = sigsafe_read(82, buf, sizeof(buf)-1);
    printf("read returned: %d\n", retval);
    if (retval >= 0) {
        printf("buf: %s\n", buf);
    } else {
        printf("error condition %d (%s)\n", -retval, strerror(-retval));
    }
    return 0;
}
