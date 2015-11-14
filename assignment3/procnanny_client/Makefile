CC = gcc
CFLAGS  = -std=c99 -Wall -DMEMWATCH -DMW_STDIO
SRCS = memwatch.c proc_nanny_client.c linked_list.c
INCLUDES = memwatch.h proc_nanny_client.h linked_list.h

all: procnanny.client

procnanny.client: $(SRCS) $(INCLUDES)
	$(CC) $(CFLAGS) $(SRCS) -o procnanny.client
	
clean: 
	$(RM) procnanny.client test15 test5 testLong *.o *.out *.log *.tar
	
test: procnanny.client test5 test15 testLong
	$(info test programs built)

test5: test5.c
	gcc -o test5 test5.c

test15: test15.c
	gcc -o test15 test15.c

testLong: test.c
	gcc -o testLong test.c

tar:
	tar cfv submit.tar README.md Makefile proc_nanny_client.c proc_nanny_client.h linked_list.c linked_list.h
