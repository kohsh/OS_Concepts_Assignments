#ifndef PROC_NANNY_H_
#define PROC_NANNY_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

#define LOG_MESSAGE_LENGTH 512
#define MAX_PROCESSES 512

typedef struct LogMessage {
    time_t time;
    char message[LOG_MESSAGE_LENGTH];
} LogMessage;

typedef struct Pipe {
    int readWrite[2]; // read 0, write 1
} Pipe;

int pnMain(int argc, char* argv[]);

void killAllProcNannys();

void processConfigFile(const char *configurationFile);

// returns pid of child procNanny
__pid_t forkAndMonitorProcess(const char *process, unsigned int monitorTime);

void getPids(const char* processName, pid_t pids[MAX_PROCESSES]);

void monitorProcess(const char *process, unsigned int monitorTime);

void trimWhitespace(char* str);

void freeConfigLines();

void nannyLog(const char* message);

void exitError(const char* errorMessage);

void runAndPrint(const char* command);

int getNumberOfLines(FILE* stream);

#endif /* PROC_NANNY_H_ */
