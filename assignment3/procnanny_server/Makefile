CC = gcc
CFLAGS = -std=c99 -Wall -DMEMWATCH -DMW_STDIO
SRCS_SERVER = memwatch.c proc_nanny_server.c linked_list.c
SRCS_CLIENT = memwatch.c proc_nanny_client.c linked_list.c
INCLUDES_SERVER = memwatch.h proc_nanny_server.h linked_list.h
INCLUDES_CLIENT = memwatch.h proc_nanny_client.h linked_list.h

all: procnanny.server procnanny.client

procnanny.server: $(SRCS_SERVER) $(INCLUDES_SERVER)
	$(CC) $(CFLAGS) $(SRCS_SERVER) -o procnanny.server

procnanny.client: $(SRCS_CLIENT) $(INCLUDES_CLIENT)
	$(CC) $(CFLAGS) $(SRCS_CLIENT) -o procnanny.client
	
clean: 
	$(RM) procnanny.server procnanny.client test15 test5 testLong *.o *.out *.log *.tar *.info
	
test: procnanny.server procnanny.client test5 test15 testLong
	$(info test programs built)

test5: test5.c
	gcc -o test5 test5.c

test15: test15.c
	gcc -o test15 test15.c

testLong: test.c
	gcc -o testLong test.c

tar:
	tar cfv submit.tar README.md Makefile proc_nanny_server.c proc_nanny_server.h proc_nanny_client.c proc_nanny_client.h linked_list.c linked_list.h
