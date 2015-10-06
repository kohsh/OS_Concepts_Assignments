CC = gcc
CFLAGS  = -std=c99 -Wall -DMEMWATCH -DMW_STDIO
SRCS = main.c memwatch.c proc_nanny.c
INCLUDES = proc_nanny.h memwatch.h

all: procnanny

procnanny: $(SRCS) $(INCLUDES)
	$(CC) $(CFLAGS) $(SRCS) -o procnanny
	
clean: 
	$(RM) procnanny test15 test5 *.o *.out *.log *.tar
	
test: procnanny test5 test15
	chmod +x launchTestProcesses.sh
	./launchTestProcesses.sh&
	PROCNANNYLOGS="./pn.log" ./procnanny testInput.config
	cat pn.log

test5: test5.c
	gcc -o test5 test5.c

test15: test15.c
	gcc -o test15 test15.c

tar:
	tar cfv submit.tar Makefile main.c proc_nanny.c proc_nanny.h
