CC = gcc
CFLAGS  = -std=c99 -Wall -DMEMWATCH -DMW_STDIO
SRCS = main.c memwatch.c proc_nanny.c
INCLUDES = proc_nanny.h memwatch.h

all: procnanny

procnanny: $(SRCS) $(INCLUDES)
	$(CC) $(CFLAGS) $(SRCS) -o procnanny
	
clean: 
	$(RM) procnanny *.o *.out *.log
	
run: procnanny
	./procnanny
