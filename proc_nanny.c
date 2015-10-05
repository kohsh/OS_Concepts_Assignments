#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <sys/wait.h>
#include "proc_nanny.h"
#include "memwatch.h"

Pipe totalKilledProccesses;
Pipe logMessages;

char logLocation[512];
char* configLines[CONFIG_FILE_LINES] = {NULL};
__pid_t childPids[CONFIG_FILE_LINES] = {-1};
int numberChildren = 0;

int pnMain(int args, char* argv[]) {

    if (args <= 1) {
		fputs("ERROR: No input file indicated as argument.\n", stderr);
		fflush(stdout);
		exit(EXIT_FAILURE);
	}

    char * temp = getenv("PROCNANNYLOGS");
    snprintf(logLocation, 512, "%s", temp);

    killAllProcNannys();
    processConfigFile(argv[1]);
    freeConfigLines();

    exit(EXIT_SUCCESS);
}

void killAllProcNannys() {
    pid_t pids[MAX_PROCESSES] = {-1};
    getPids("procnanny", pids);
    for(int i = 0; i < MAX_PROCESSES; i++) {
        if (pids[i] > 0 && pids[i] != getpid()) {
            if(kill(pids[i], 0) == 0) {
                kill(pids[i], SIGKILL);
            }
        }
    }
}

void processConfigFile(const char *configurationFile) {
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t charsRead;

    fp = fopen(configurationFile, "r");
    if (fp == NULL)
        exitError("ERROR: could not read configuration file.");

    int index = 0;

    while ((charsRead = getline(&line, &len, fp)) != -1) {
        configLines[index] = (char *) calloc((size_t) charsRead + 1,  sizeof(char));
        strncpy(configLines[index], line, (size_t) charsRead);
        trimWhitespace(configLines[index]);
        index++;
    }
    fclose(fp);
    unsigned int monitorTime = (unsigned int) atoi(configLines[0]);

    if (pipe(totalKilledProccesses.readWrite) != 0) {
        exitError("pipe error");
    }

    if (pipe(logMessages.readWrite) != 0) {
        exitError("pipe error");
    }

    printf("My pid is: %d\n", getpid());


    for (int i = 1; i <CONFIG_FILE_LINES; i++) {
        if (configLines[i] != NULL) {
            childPids[i-1] = forkAndMonitorProcess(configLines[i], monitorTime);
            numberChildren++;
        }
    }

//    todo: begin reading pipe results from children -> log Messages and tally the killed processes
    readPipes();
}

__pid_t forkAndMonitorProcess(const char *process, unsigned int monitorTime) {
    __pid_t forkResult = fork();

    switch(forkResult) {
        case -1:    //Error
            exitError("ERROR: error in monitoring process");
            break;
        case 0:     //Child
            monitorProcess(process, monitorTime);
            break;
        default:    //Parent
            break;
    }

    return forkResult;
}

void monitorProcess(const char *process, unsigned int monitorTime) {
    close(logMessages.readWrite[READ_PIPE]);
    close(totalKilledProccesses.readWrite[READ_PIPE]);

    pid_t pids[MAX_PROCESSES] = {-1};
    getPids(process, pids);
    int numberKilledProcesses = 0;
    int numberFoundProceses = 0;
    char timebuffer[50];

    for(int i = 0; i < MAX_PROCESSES; i++) {

        if (pids[i] > 0) {
            LogMessage logMsg;
            time(&logMsg.time);
            snprintf(timebuffer, 50, "%s", ctime(&logMsg.time));
            trimWhitespace(timebuffer);
            snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                     "[%s] Info: Initializing monitoring of process '%s' (PID %d).\n",
                     timebuffer, process, pids[i]);
            ctime(&logMsg.time);
            writeToPipe(&logMessages, logMsg.message);
            numberFoundProceses++;
        }
    }

    if (numberFoundProceses == 0) {
        LogMessage msg;
        time(&msg.time);
        snprintf(timebuffer, 50, "%s", ctime(&msg.time));
        trimWhitespace(timebuffer);
        snprintf(msg.message, LOG_MESSAGE_LENGTH, "[%s] Info: No '%s' processes found.\n"
                ,timebuffer, process);
        writeToPipe(&logMessages, msg.message);
        close(logMessages.readWrite[WRITE_PIPE]);
        close(totalKilledProccesses.readWrite[WRITE_PIPE]);
        freeConfigLines();
        _exit(0);
    }

    sleep(monitorTime);

    for(int i = 0; i < MAX_PROCESSES; i++) {

        if (pids[i] > 0 && pids[i] != getpid()) {
            if(kill(pids[i], 0) == 0) {

                kill(pids[i], SIGKILL);

                LogMessage logMsg;
                time(&logMsg.time);
                snprintf(timebuffer, 50, "%s", ctime(&logMsg.time));
                trimWhitespace(timebuffer);

                snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                         "[%s] Action: PID %d (%s) killed after exceeding %d seconds.\n",
                         timebuffer, pids[i], process, monitorTime);
                writeToPipe(&logMessages, logMsg.message);
                numberKilledProcesses++;
            }
        }
    }

    close(logMessages.readWrite[WRITE_PIPE]);

    char numberBuffer[20];
    snprintf(numberBuffer, 19, "%d\n", numberKilledProcesses);
    writeToPipe(&totalKilledProccesses, numberBuffer);
    close(totalKilledProccesses.readWrite[WRITE_PIPE]);


    freeConfigLines();
    _exit(0);
}

void writeToPipe(Pipe *pPipe, const char *message) {
    write(pPipe->readWrite[WRITE_PIPE], message, strlen(message) + 1);
    printf("%s", message);
    fflush(stdout);
}

void readPipes() {
    close(logMessages.readWrite[WRITE_PIPE]);           // don't need to write to the pipe
    close(totalKilledProccesses.readWrite[WRITE_PIPE]); // don't need to write to the pipe

    char byte;

    while (read(logMessages.readWrite[READ_PIPE], &byte, 1) != 0) {
        FILE* log = fopen(logLocation, "a");
        fputc(byte, log);
        fclose(log);
    }

    close(logMessages.readWrite[READ_PIPE]);

//    FILE *fp = fdopen(totalKilledProccesses.readWrite[READ_PIPE], "r");
//    char number[255];
//    fgets(number, 255, fp);
//    printf("%s", number);

    printf("made it out");
    fflush(stdout);
}

void exitError(const char *errorMessage) {
    fputs(errorMessage, stderr);
    exit(EXIT_FAILURE);
}

void trimWhitespace(char *str) {
    int i;
    int begin = 0;
    size_t end = strlen(str) - 1;

    while (isspace(str[begin]))
        begin++;

    while ((end >= begin) && isspace(str[end]))
        end--;

    // Shift all characters back to the start of the string array.
    for (i = begin; i <= end; i++)
        str[i - begin] = str[i];

    str[i - begin] = '\0'; // Null terminate string.

}

void freeConfigLines() {
    for (int i = 0; i < CONFIG_FILE_LINES; i++) {
        if (configLines[i] != NULL) {
            free(configLines[i]);
            configLines[i] = NULL;
        }
    }
}

void getPids(const char *processName, pid_t pids[MAX_PROCESSES]) {
    char command[512];
    snprintf(command, 511, "pgrep '%s'", processName);
    FILE* pgrepOutput = popen(command, "r");

    char * line = NULL;
    size_t len = 0;
    if (pgrepOutput == NULL)
        return;

    int index = 0;

    while (getline(&line, &len, pgrepOutput) != -1) {
        pids[index] = (pid_t) atoi(line);
        index++;
    }

    pclose(pgrepOutput);
}


