CFLAGS=		-Wall -I.. -g
LIBS=		-lpthread
OBJS=		test_main.o sigsafe_read.o sighandler_platform.o ../sigsafe.o
LDFLAGS=	-g

test_main: $(OBJS)
	gcc $(LDFLAGS) -o test_main $(OBJS) $(LIBS)

.s.o:
	gcc -Wall -c $<
