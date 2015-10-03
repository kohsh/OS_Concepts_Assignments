#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "proc_nanny.h"
#include "memwatch.h"

#define CONFIG_FILE_LINES 128

char* configLines[CONFIG_FILE_LINES] = {NULL};
Pipe pipes[CONFIG_FILE_LINES];
__pid_t childPids[CONFIG_FILE_LINES] = {-1};

int pnMain(int args, char* argv[]) {

    if (args <= 1) {
		fputs("ERROR: No input file supplied.\n", stderr);
		fflush(stdout);
		exit(EXIT_FAILURE);
	}

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

    unsigned int monitorTime = (unsigned int) atoi(configLines[0]);

    for (int i = 1; i <CONFIG_FILE_LINES; i++) {
        if (configLines[i] != NULL) {
            childPids[i-1] = forkAndMonitorProcess(configLines[i], monitorTime);
            printf("new procnanny: pid=%d\n", childPids[i-1]);
            fflush(stdout);
        }
    }

    // todo: wait to get pipe results from children and log results

    printf("waiting for child processes\n");
    fflush(stdout);
    for (int i = 0; i < CONFIG_FILE_LINES; i++) {
        if (childPids[i] > 0) {
            printf("Hello!!1\n");
            fflush(stdout);
            int times = 0;
            while(times < monitorTime*2) {
                int status;
                waitpid(childPids[i], &status, WNOHANG);
                sleep(5);
                times+=5;
            }
        }
    }
    printf("child processes done!\n");
    fflush(stdout);



    fclose(fp);
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
    pid_t pids[MAX_PROCESSES] = {-1};
    getPids(process, pids);
    int numberKilledProcesses = 0;

    for(int i = 0; i < MAX_PROCESSES; i++) {
        if (pids[i] > 0) {
            LogMessage logMsg;
            snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                     "Info: Initializing monitoring of process '%s' (PID %d).", process, pids[i]);
            time(&logMsg.time);

            printf("%s\n", logMsg.message);
            fflush(stdout);
            /// todo: do something with these messages
        }
    }

    sleep(monitorTime);

    for(int i = 0; i < MAX_PROCESSES; i++) {
        if (pids[i] != -1) {
            if(kill(pids[i], 0) == 0) {
                kill(pids[i], SIGKILL);
                LogMessage logMsg;
                snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                         "Action: PID %d (%s) killed after exceeding %d seconds",
                         pids[i], process, monitorTime);
                time(&logMsg.time);
                printf("%s\n", logMsg.message);
                fflush(stdout);
                /// todo: do something with these messages

                numberKilledProcesses++;
            }
        }
    }

    //todo: pipe the number of killed process back to the parent process

    freeConfigLines();
    exit(EXIT_SUCCESS);
}

void exitError(const char *errorMessage) {
    fputs(errorMessage, stderr);
    exit(EXIT_FAILURE);
}

void runAndPrint(const char* command) {
    FILE* in = popen(command, "r");
    char buff[2048];

    if (in == NULL) {
        pclose(in);
        return;
    }

    while(fgets(buff, sizeof(buff), in)!=NULL){
        printf("%s", buff);
    }
    pclose(in);
}

int getNumberOfLines(FILE *stream) {
    int character = fgetc(stream);
    int lines = 0;

    while (character != EOF) {
        if (character == '\n') {
            lines++;
        }
        character = fgetc(stream);
    }

    return lines;
}

void nannyLog(const char *message) {
    printf("%s", message);
    fflush(stdout);
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
