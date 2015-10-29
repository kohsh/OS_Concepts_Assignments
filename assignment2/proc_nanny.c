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
#include <sys/poll.h>
#include <fcntl.h>
#include "proc_nanny.h"
#include "linked_list.h"
#include "memwatch.h"

bool receivedSIGHUP = false;
bool receivedSIGINT = false;
bool receivedSIGALARM = false;

int numProcessesKilled = 0;
Pipe totalKilledProcesses;
Pipe logMessages;

char logLocation[512];
char configFileLocation[512];

ProgramConfig configLines[CONFIG_FILE_LINES];
List monitoredProccesses;
List childProccesses;

int pnMain(int args, char* argv[]) {

    if (signal(SIGHUP, &signalHandler) == SIG_ERR)
        printf("error with catching SIGHUP\n");

    if (signal(SIGINT, &signalHandler) == SIG_ERR)
        printf("error with catching SIGINT\n");

    if (signal(SIGALRM, &signalHandler) == SIG_ERR)
        printf("error with setting Alarm\n");

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
    if (fp == NULL) {
        logToFile("Error", "Could not read configuration file.", true);
        exit(EXIT_FAILURE);
    }

    int index = 0;

    while ((charsRead = getline(&line, &len, fp)) != -1) {
        char tempBuff[512];
        strncpy(tempBuff, line, (size_t) charsRead);
        int numMatched = sscanf(tempBuff, "%s %d", configLines[index].programName, &configLines[index].runtime);
        if (numMatched != 2) {
            LogMessage msg;
            snprintf(msg.message, LOG_MESSAGE_LENGTH,
                     "Expected two configuration arguments at line %d of %s.",
                     index, configFileLocation);
            logToFile("Error", msg.message, false);
            cleanUp();
            exit(EXIT_FAILURE);
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

    ll_init(&monitoredProccesses, sizeof(MonitoredProcess), &monitoredProccessComparator);
    ll_init(&childProccesses, sizeof(ChildProcess), NULL);
    checkForNewMonitoredProcesses();
    alarm(REFRESH_RATE);

    while(true) {
        ll_forEach(&monitoredProccesses, &monitorNewProccesses);
        ll_forEach(&childProccesses, &checkOnChild);

        if (receivedSIGHUP) {
            receivedSIGHUP = false;
            for (int i = 0; i < CONFIG_FILE_LINES; i++) {
                configLines[i].runtime = 0;
                strcpy(configLines[i].programName, "");
            }
            readConfigurationFile();
            LogMessage msg;
            snprintf(msg.message, LOG_MESSAGE_LENGTH,
                     "Caught SIGHUP. Configuration file '%s' re-read.",
                     configFileLocation);
            char type[] = "Info";
            logToFile(type, msg.message, true);
        }

        if (receivedSIGALARM) {
            receivedSIGALARM = false;
            checkForNewMonitoredProcesses();
            alarm(REFRESH_RATE);
        }

        if (receivedSIGINT) {
            receivedSIGINT = false;
            cleanUp();
            LogMessage msg;
            snprintf(msg.message, LOG_MESSAGE_LENGTH,
                     "Caught SIGINT. Exiting cleanly. %d process(es) killed.",
                     numProcessesKilled);
            logToFile("Info", msg.message, true);
            exit(EXIT_SUCCESS);
        }
    }
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
    ll_forEach(&childProccesses, &killChild);
    ll_free(&monitoredProccesses);
    ll_free(&childProccesses);
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

bool monitoredProccessComparator(void *mp1, void *mp2) {
    MonitoredProcess* first = (MonitoredProcess*) mp1;
    MonitoredProcess* second = (MonitoredProcess*) mp2;

    if (first->processPid == second->processPid) {
        return true;
    }

    return false;
}

void signalHandler(int signo)
{
    switch (signo) {
        case SIGINT:
            receivedSIGINT = true;
            break;
        case SIGHUP:
            receivedSIGHUP = true;
            break;
        case SIGALRM:
            receivedSIGALARM = true;
            alarm(REFRESH_RATE);
            break;
        default:
            break;
    }
}

void checkForNewMonitoredProcesses() {
    for (int i = 0; i < CONFIG_FILE_LINES; i++) {
        if (strlen(configLines[i].programName) != 0) {
            pid_t pids[MAX_PROCESSES] = {-1};
            getPids(configLines[i].programName, pids);
            int numberFound = 0;
            for (int j = 0; j < MAX_PROCESSES; j++) {
                if (pids[j] > 0) {
                    MonitoredProcess temp;
                    strncpy(temp.processName, configLines[i].programName, PROGRAM_NAME_LENGTH);
                    temp.processPid = pids[j];
                    temp.runtime = configLines[i].runtime;
                    temp.beingMonitored = false;
                    ll_add_unique(&monitoredProccesses, &temp);
                    numberFound++;
                }
            }
            // TODO : determine if this is needed
//            if (numberFound == 0) {
//                LogMessage msg;
//                snprintf(msg.message, LOG_MESSAGE_LENGTH, "No '%s' processes found."
//                        , configLines[i].programName);
//                logToFile("Info", msg.message, false);
//            }
        }
    }
}

void monitorNewProccesses(void* monitoredProcess) {
    MonitoredProcess* process = (MonitoredProcess*) monitoredProcess;
    if (process->beingMonitored == false) {
        ChildProcess* worker = ll_getIf(&childProccesses, &getChildPredicate);
        if (worker == NULL) {
            worker = spawnNewChildWorker();
        }
        initializeChild(worker, process);
        LogMessage msg;
        snprintf(msg.message, LOG_MESSAGE_LENGTH, "Initializing monitoring of process '%s' (PID %d).",
                 process->processName, process->processPid);
        logToFile("Info", msg.message, false);
    }
}

bool getChildPredicate(void *childProcess) {
    ChildProcess* temp = (ChildProcess*) childProcess;
    return temp->isAvailable;
}

void initializeChild(ChildProcess *childWorker, MonitoredProcess *processToBeMonitored) {
    childWorker->isAvailable = false;
    processToBeMonitored->beingMonitored = true;
    strncpy(childWorker->processName, processToBeMonitored->processName, PROGRAM_NAME_LENGTH);
    childWorker->processPid = processToBeMonitored->processPid;
    childWorker->runtime = processToBeMonitored->runtime;

    char buff[255];
    snprintf(buff, 255, "PID: %d, RUNTIME: %d\n", processToBeMonitored->processPid, processToBeMonitored->runtime);

    write(childWorker->toChild.readWrite[WRITE_PIPE], buff, strlen(buff));
}

ChildProcess *spawnNewChildWorker() {
    ChildProcess worker;
    worker.isAvailable = true;
    pipe(worker.toChild.readWrite);
    pipe(worker.toParent.readWrite);
    __pid_t forkResult = fork();

    switch(forkResult) {
        case -1:    //Error
            exitError("ERROR: error in monitoring process");
            break;
        case 0:     //Child
            close(worker.toChild.readWrite[WRITE_PIPE]);
            close(worker.toParent.readWrite[READ_PIPE]);
            ll_free(&monitoredProccesses);
            ll_free(&childProccesses);
            while(true) {
                FILE* fromParent = fdopen(worker.toChild.readWrite[READ_PIPE], "r");
                char command[255];
                while(fgets(command, 255, fromParent) != NULL) {
                    // get parameters from parent's command
                    pid_t pidToMonitor = -1;
                    unsigned int runtime = 0;
                    int numKilled = 0;
                    sscanf(command, "PID: %d, RUNTIME: %d\n", &pidToMonitor, &runtime);

                    // monitor process

                    sleep(runtime);

                    if(kill(pidToMonitor, 0) == 0) {
                        killPid(pidToMonitor);
                        numKilled = 1;
                    }
                    char buff[5];
                    snprintf(buff, 5, "%d", numKilled);
                    write(worker.toParent.readWrite[WRITE_PIPE], buff, strlen(buff));
                }
                fclose(fromParent);
                exit(EXIT_SUCCESS);
            }
            break;
        default:    //Parent
            break;
    }

    close(worker.toChild.readWrite[READ_PIPE]);
    close(worker.toParent.readWrite[WRITE_PIPE]);

    //set to non blocking read of writes from child
    int flags = fcntl(worker.toParent.readWrite[READ_PIPE], F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(worker.toParent.readWrite[READ_PIPE], F_SETFL, flags);


    worker.childPid = forkResult;
    ll_add(&childProccesses, &worker);
    ChildProcess* retVal = (ChildProcess*) ll_getIf(&childProccesses, &getChildPredicate);
    return retVal;

}

void checkOnChild(void *childProcess) {
    ChildProcess* child = (ChildProcess*) childProcess;
    if (child->isAvailable == false) {
        char command[255];
        if (read(child->toParent.readWrite[READ_PIPE], command, 255) == -1) {
            return;
        }
        int numKilled = 0;
        sscanf(command, "%d\n", &numKilled);
        if (numKilled != 0) {
            numProcessesKilled+=numKilled;
            LogMessage msg;
            snprintf(msg.message, LOG_MESSAGE_LENGTH, "PID %d (%s) killed after exceeding %d seconds.",
                     child->processPid, child->processName, child->runtime);
            logToFile("Action", msg.message, false);
        }
        child->isAvailable = true;
        MonitoredProcess temp;
        temp.processPid = child->processPid;
        ll_remove(&monitoredProccesses, &temp);
    }
}

void killChild(void *childProcess) {
    ChildProcess* child = (ChildProcess*) childProcess;
    killPid(child->childPid);
}

void logToFile(const char* type, const char* msg, bool logToSTDOUT) {
    char timebuffer[TIME_BUFFER_SIZE];
    getCurrentTime(timebuffer);
    LogMessage logMsg;
    snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
             "[%s] %s: %s\n",
             timebuffer, type, msg);

    FILE* log = fopen(logLocation, "a");
    fprintf(log, "%s", logMsg.message);
    fclose(log);

    if (logToSTDOUT == true) {
        printf("%s", logMsg.message);
    }
}
