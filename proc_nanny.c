/**
 * Copyright 2015 Kyle O'Shaughnessy
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include "proc_nanny.h"
#include "memwatch.h"

Pipe totalKilledProcesses;
Pipe logMessages;

char logLocation[512];
char* configLines[CONFIG_FILE_LINES] = {NULL};

int pnMain(int args, char* argv[]) {
    checkInputs(args, argv);
    killAllProcNannys();
    beginProcNanny(argv[1]);
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

void beginProcNanny(const char *configurationFile) {
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

    if (pipe(totalKilledProcesses.readWrite) != 0) {
        exitError("pipe error");
    }

    if (pipe(logMessages.readWrite) != 0) {
        exitError("pipe error");
    }

    for (int i = 1; i <CONFIG_FILE_LINES; i++) {
        if (configLines[i] != NULL) {
            forkMonitorProcess(configLines[i], monitorTime);
        }
    }

    readPipes();
}

void forkMonitorProcess(const char *process, unsigned int monitorTime) {
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
}

void monitorProcess(const char *process, unsigned int monitorTime) {
    close(logMessages.readWrite[READ_PIPE]);
    close(totalKilledProcesses.readWrite[READ_PIPE]);

    pid_t processPids[MAX_PROCESSES] = {-1};
    getPids(process, processPids);
    int numberKilledProcesses = 0;
    int numberFoundProcesses = 0;

    for(int i = 0; i < MAX_PROCESSES; i++) {

        if (processPids[i] > 0) {
            char timeBuffer[TIME_BUFFER_SIZE];
            getCurrentTime(timeBuffer);
            LogMessage logMsg;
            snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                     "[%s] Info: Initializing monitoring of process '%s' (PID %d).\n",
                     timeBuffer, process, processPids[i]);
            writeToPipe(&logMessages, logMsg.message);
            numberFoundProcesses++;
        }
    }

    if (numberFoundProcesses == 0) {
        char timeBuffer[TIME_BUFFER_SIZE];
        getCurrentTime(timeBuffer);
        LogMessage msg;
        snprintf(msg.message, LOG_MESSAGE_LENGTH, "[%s] Info: No '%s' processes found.\n"
                , timeBuffer, process);
        writeToPipe(&logMessages, msg.message);
        close(logMessages.readWrite[WRITE_PIPE]);
        close(totalKilledProcesses.readWrite[WRITE_PIPE]);
        freeConfigLines();
        _exit(0);
    }

    sleep(monitorTime);

    for(int i = 0; i < MAX_PROCESSES; i++) {

        if (processPids[i] > 0 && processPids[i] != getpid()) {
            if(kill(processPids[i], 0) == 0) {
                kill(processPids[i], SIGKILL);
                char timeBuffer[TIME_BUFFER_SIZE];
                getCurrentTime(timeBuffer);
                LogMessage logMsg;

                snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                         "[%s] Action: PID %d (%s) killed after exceeding %d seconds.\n",
                         timeBuffer, processPids[i], process, monitorTime);
                writeToPipe(&logMessages, logMsg.message);
                numberKilledProcesses++;
            }
        }
    }

    close(logMessages.readWrite[WRITE_PIPE]);

    char numberBuffer[20];
    snprintf(numberBuffer, 20, "%d\n", numberKilledProcesses);
    writeToPipe(&totalKilledProcesses, numberBuffer);
    close(totalKilledProcesses.readWrite[WRITE_PIPE]);

    freeConfigLines();
    _exit(0);
}

void writeToPipe(Pipe *pPipe, const char *message) {
    write(pPipe->readWrite[WRITE_PIPE], message, strlen(message));
}

void readPipes() {
    close(logMessages.readWrite[WRITE_PIPE]);           // don't need to write to the pipe
    close(totalKilledProcesses.readWrite[WRITE_PIPE]); // don't need to write to the pipe

    char byte;
    FILE* log = fopen(logLocation, "a");

    while (read(logMessages.readWrite[READ_PIPE], &byte, 1) != 0) {
        fputc(byte, log);
    }

    fclose(log);
    close(logMessages.readWrite[READ_PIPE]);

    FILE*totalKilledFP = fdopen(totalKilledProcesses.readWrite[READ_PIPE], "r");
    char numberString[10];
    int numberInteger = 0;
    int processesKilled = 0;
    while(fgets(numberString, 10, totalKilledFP) != NULL) {
        sscanf(numberString, "%d", &numberInteger);
        processesKilled += numberInteger;
        numberInteger = 0;
    }

    char timebuffer[TIME_BUFFER_SIZE];
    getCurrentTime(timebuffer);
    LogMessage logMsg;
    snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
             "[%s] Info: Exiting. %d process(es) killed.\n",
             timebuffer, processesKilled);

    log = fopen(logLocation, "a");
    fprintf(log, "%s", logMsg.message);
    fclose(log);
    fclose(totalKilledFP);
    close(totalKilledProcesses.readWrite[READ_PIPE]);
}

void freeConfigLines() {
    for (int i = 0; i < CONFIG_FILE_LINES; i++) {
        if (configLines[i] != NULL) {
            free(configLines[i]);
            configLines[i] = NULL;
        }
    }
}

void exitError(const char *errorMessage) {
    fputs(errorMessage, stderr);
    exit(EXIT_FAILURE);
}

void trimWhitespace(char *str) {
    int i;
    int begin = 0;
    size_t end = strlen(str) - 1;

    while (isspace(str[begin])) {
        begin++;
    }

    while ((end >= begin) && isspace(str[end])) {
        end--;
    }

    for (i = begin; i <= end; i++) {
        str[i - begin] = str[i];
    }

    str[i - begin] = '\0';

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

void getCurrentTime(char *buffer) {
    time_t rawTime;
    struct tm *timeInfo;
    time (&rawTime);
    timeInfo = localtime (&rawTime);
    strftime(buffer,TIME_BUFFER_SIZE,"%a %b %d %H:%M:%S %Z %Y", timeInfo);
    trimWhitespace(buffer);
}

void checkInputs(int args, char* argv[]) {
    if (args <= 1) {
        exitError("ERROR: No input file indicated as argument.\n");
    }

    char * temp = getenv("PROCNANNYLOGS");

    if (temp == NULL) {
        exitError("ERROR: Environment variable 'PROCNANNYLOGS' not specified.\n");
    }
    else {
        snprintf(logLocation, 512, "%s", temp);
    }
}
