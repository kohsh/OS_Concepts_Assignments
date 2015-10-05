#ifndef PROC_NANNY_H_
#define PROC_NANNY_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

#define MAX_PROCESSES 1024
#define CONFIG_FILE_LINES 256
#define LOG_MESSAGE_LENGTH 512

#define READ_PIPE 0
#define WRITE_PIPE 1

typedef struct Pipe {
    int readWrite[2]; // read 0, write 1
} Pipe;

typedef struct LogMessage {
    time_t time;
    char message[LOG_MESSAGE_LENGTH];
} LogMessage;

int pnMain(int argc, char* argv[]);

void killAllProcNannys();
void beginNanny(const char *configurationFile);
void getPids(const char* processName, pid_t pids[MAX_PROCESSES]);
void monitorProcess(const char *process, unsigned int monitorTime);
void trimWhitespace(char* str);
void freeConfigLines();
void exitError(const char* errorMessage);
void writeToPipe(Pipe* pPipe, const char* message);
void readPipes();

__pid_t forkMonitorProcess(const char *process, unsigned int monitorTime);

#endif /* PROC_NANNY_H_ */
