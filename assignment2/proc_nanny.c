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
#include "linked_list.h"
#include "memwatch.h"

Pipe totalKilledProcesses;
Pipe logMessages;

char logLocation[512];
char configFileLocation[512];

ProgramConfig configLines[CONFIG_FILE_LINES];
List monitoredProccesses;

int pnMain(int args, char* argv[]) {
    checkInputs(args, argv);
    killAllProcNannys();
    readConfigurationFile();
    beginProcNanny();
    cleanUp();
    exit(EXIT_SUCCESS);
}

void killAllProcNannys() {
    pid_t pids[MAX_PROCESSES] = {-1};
    getPids("procnanny", pids);
    for(int i = 0; i < MAX_PROCESSES; i++) {
        if (pids[i] > 0 && pids[i] != getpid()) {
            if(kill(pids[i], 0) == 0) {
                killPid(pids[i]);
            }
        }
    }
}

void readConfigurationFile() {
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t charsRead;

    fp = fopen(configFileLocation, "r");
    if (fp == NULL)
        exitError("ERROR: could not read configuration file.");

    int index = 0;

    while ((charsRead = getline(&line, &len, fp)) != -1) {
        char tempBuff[512];
        strncpy(tempBuff, line, (size_t) charsRead);
        int numMatched = sscanf(tempBuff, "%s %d", configLines[index].programName, &configLines[index].runtime);
        if (numMatched != 2) {
            // "expected to match only 2 arguments, matched %d", index
            // "here is line %d: %s", index, tempBuff
            //logConfigFileError(index, tempBuff, numMatched)
        }
        trimWhitespace(configLines[index].programName);
        index++;
    }
    fclose(fp);
}

void beginProcNanny() {

    if (pipe(totalKilledProcesses.readWrite) != 0) {
        exitError("pipe error");
    }

    if (pipe(logMessages.readWrite) != 0) {
        exitError("pipe error");
    }

    ll_init(&monitoredProccesses, sizeof(MonitoredProcess));

    //given the configuration lines, find all processes matching the program names (use getPids)
    //and delegate these off to new instances of child processes (linked list i guess)
    //

    for (int i = 0; i <CONFIG_FILE_LINES; i++) {
        if (strlen(configLines[i].programName) != 0) {
            pid_t pids[MAX_PROCESSES] = {-1};
            getPids(configLines[i].programName, pids);
            for (int j = 0; j < MAX_PROCESSES; j++) {
                if (pids[j] > 0) {
                    MonitoredProcess temp;
                    strncpy(temp.processName, configLines[i].programName, PROGRAM_NAME_LENGTH);
                    temp.processPid = pids[j];
                    temp.runtime = configLines[i].runtime;
                    ll_add(&monitoredProccesses, &temp); // in the future change this to ll_add_unique
                }
            }
        }
    }




//    for (int i = 0; i <CONFIG_FILE_LINES; i++) {
//        if (strlen(configLines[i].programName) != 0) {
//            forkMonitorProcess(configLines[i].programName, configLines[i].runtime);
//        }
//    }

   // readPipes();
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
        cleanUp();
        _exit(0);
    }

    sleep(monitorTime);

    for(int i = 0; i < MAX_PROCESSES; i++) {

        if (processPids[i] > 0 && processPids[i] != getpid()) {
            if(kill(processPids[i], 0) == 0) {
                killPid(processPids[i]);
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

    cleanUp();
    _exit(0);
}

void writeToPipe(Pipe *pPipe, const char *message) {
    write(pPipe->readWrite[WRITE_PIPE], message, strlen(message));
}

void readPipes() {
    close(logMessages.readWrite[WRITE_PIPE]);          // don't need to write to the pipe
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

void cleanUp() {
//    for (int i = 0; i < CONFIG_FILE_LINES; i++) {
//        if (configLines[i] != NULL) {
//            free(configLines[i]);
//            configLines[i] = NULL;
//        }
//    }
    ll_free(&monitoredProccesses);
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
    snprintf(command, 511, "pgrep -x '%s'", processName);
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
    char * temp = getenv("PROCNANNYLOGS");

    if (temp == NULL) {
        snprintf(logLocation, 512, "./procnanny.log");
        char timebuffer[TIME_BUFFER_SIZE];
        getCurrentTime(timebuffer);
        LogMessage logMsg;
        snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                 "[%s] Error: Environment variable 'PROCNANNYLOGS' not specified.\n",
                 timebuffer);
        FILE* log = fopen(logLocation, "a");
        fprintf(log, "%s", logMsg.message);
        fclose(log);
    }
    else {
        snprintf(logLocation, 512, "%s", temp);
    }

    if (args <= 1) {
        char timebuffer[TIME_BUFFER_SIZE];
        getCurrentTime(timebuffer);
        LogMessage logMsg;
        snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                 "[%s] Error: procnanny configuration file not provided as argument.\n",
                 timebuffer);
        FILE* log = fopen(logLocation, "a");
        fprintf(log, "%s", logMsg.message);
        fclose(log);
        exit(EXIT_FAILURE);
    }

    if (access(argv[1], R_OK) == -1) {
        char timebuffer[TIME_BUFFER_SIZE];
        getCurrentTime(timebuffer);
        LogMessage logMsg;
        snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                 "[%s] Error: Unable to read from configuration file (%s).\n",
                 timebuffer, argv[1]);
        FILE* log = fopen(logLocation, "a");
        fprintf(log, "%s", logMsg.message);
        fclose(log);
        exit(EXIT_FAILURE);
    }

    // cache the location of the configuration file
    strncpy(configFileLocation, argv[1], 512);
}

void killPid(pid_t pid) {
    char buff[256];
    snprintf(buff, 256, "kill -9 %d > /dev/null", pid);
    system(buff);
}