CC = gcc
CFLAGS  = -std=c99 -Wall -DMEMWATCH -DMW_STDIO
SRCS = main.c memwatch.c proc_nanny.c linked_list.c
INCLUDES = proc_nanny.h memwatch.h linked_list.h

all: procnanny

procnanny: $(SRCS) $(INCLUDES)
	$(CC) $(CFLAGS) $(SRCS) -o procnanny
	
clean: 
	$(RM) procnanny test15 test5 testLong *.o *.out *.log *.tar
	
test: procnanny test5 test15 testLong
	$(info test programs built)

test5: test5.c
	gcc -o test5 test5.c

test15: test15.c
	gcc -o test15 test15.c

testLong: test.c
	gcc -o testLong test.c

tar:
	tar cfv submit.tar README.md Makefile main.c proc_nanny.c proc_nanny.h linked_list.c linked_list.h
