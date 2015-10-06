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

Pipe totalKilledProccesses;
Pipe logMessages;

char logLocation[512];
char* configLines[CONFIG_FILE_LINES] = {NULL};
__pid_t childPids[CONFIG_FILE_LINES] = {-1};
int numberChildren = 0;

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

    if (pipe(totalKilledProccesses.readWrite) != 0) {
        exitError("pipe error");
    }

    if (pipe(logMessages.readWrite) != 0) {
        exitError("pipe error");
    }

    for (int i = 1; i <CONFIG_FILE_LINES; i++) {
        if (configLines[i] != NULL) {
            childPids[i-1] = forkMonitorProcess(configLines[i], monitorTime);
            numberChildren++;
        }
    }

//    todo: begin reading pipe results from children -> log Messages and tally the killed processes
    readPipes();
}

__pid_t forkMonitorProcess(const char *process, unsigned int monitorTime) {
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

    for(int i = 0; i < MAX_PROCESSES; i++) {

        if (pids[i] > 0) {
            char timebuffer[TIME_BUFFER_SIZE];
            getCurrentTime(timebuffer);
            LogMessage logMsg;
            snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                     "[%s] Info: Initializing monitoring of process '%s' (PID %d).\n",
                     timebuffer, process, pids[i]);
            ctime(&logMsg.time);
            writeToPipe(&logMessages, logMsg.message);
            numberFoundProceses++;
        }
    }

    if (numberFoundProceses == 0) {
        char timebuffer[TIME_BUFFER_SIZE];
        getCurrentTime(timebuffer);
        LogMessage msg;
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
                char timebuffer[TIME_BUFFER_SIZE];
                getCurrentTime(timebuffer);
                LogMessage logMsg;

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
    snprintf(numberBuffer, 20, "%d\n", numberKilledProcesses);
    writeToPipe(&totalKilledProccesses, numberBuffer);
    close(totalKilledProccesses.readWrite[WRITE_PIPE]);

    freeConfigLines();
    _exit(0);
}

void writeToPipe(Pipe *pPipe, const char *message) {
    write(pPipe->readWrite[WRITE_PIPE], message, strlen(message));
}

void readPipes() {
    close(logMessages.readWrite[WRITE_PIPE]);           // don't need to write to the pipe
    close(totalKilledProccesses.readWrite[WRITE_PIPE]); // don't need to write to the pipe

    char byte;
    FILE* log = fopen(logLocation, "a");

    while (read(logMessages.readWrite[READ_PIPE], &byte, 1) != 0) {
        fputc(byte, log);
    }

    fclose(log);
    close(logMessages.readWrite[READ_PIPE]);

    FILE* totFP = fdopen(totalKilledProccesses.readWrite[READ_PIPE], "r");
    char number[10];
    int NumbersFound = 0;
    int integer = 0;
    while(fgets(number, 10, totFP) != NULL) {
        sscanf(number, "%d", &integer);
        NumbersFound+=integer;
        integer = 0;
    }

    char timebuffer[TIME_BUFFER_SIZE];
    getCurrentTime(timebuffer);
    LogMessage logMsg;
    snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
             "[%s] Info: Exiting. %d process(es) killed.\n",
             timebuffer, NumbersFound);

    log = fopen(logLocation, "a");
    fprintf(log, "%s", logMsg.message);
    fclose(log);
    fclose(totFP);
    close(totalKilledProccesses.readWrite[READ_PIPE]);
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

    while (isspace(str[begin]))
        begin++;

    while ((end >= begin) && isspace(str[end]))
        end--;

    // Shift all characters back to the start of the string array.
    for (i = begin; i <= end; i++)
        str[i - begin] = str[i];

    str[i - begin] = '\0'; // Null terminate string.

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
    time_t rawtime;
    struct tm * timeinfo;
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    strftime(buffer,TIME_BUFFER_SIZE,"%a %b %d %H:%M:%S %h %Y", timeinfo);
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
