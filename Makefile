#include lib/Makefile

CC = gcc
CFLAGS = -l pthread

OBJS = work_thread.o
PROGS = client server

all: ${OBJS} ${PROGS}

server:
	${CC} server.c ${OBJS} ${CFLAGS} -o server
	
test:
	${CC} test.c ${CFLAGS} -o test

client:
	${CC} client.c ${CFLAGS} -o client

work_thread.o:
	${CC} -c work_thread.c

clean:
	rm -f ${PROGS} ${OBJS}
	
