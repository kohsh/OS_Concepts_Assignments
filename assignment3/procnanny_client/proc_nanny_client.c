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
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "proc_nanny_client.h"
#include "linked_list.h"
#include "memwatch.h"

bool receivedSIGHUP = false;
bool receivedSIGINT = false;
bool receivedSIGALARM = false;
bool firstConfigurationReRead = false;

int numProcessesKilled = 0;

int server = 0;
int port;
char hostname[64];

ProgramConfig configLines[CONFIG_FILE_LINES];
List monitoredProcesses;
List childProcesses;

int pnMain(int args, char* argv[]) {
    if (signal(SIGALRM, &signalHandler) == SIG_ERR)
        printf("error with setting Alarm\n");

    checkInputs(args, argv);
    killAllProcNannys();
    connectToServer();
    readConfigurationFromServer(NULL);
    beginProcNanny();
    cleanUp();
    exit(EXIT_SUCCESS);
}

void signalHandler(int signo) {
    switch (signo) {
        case SIGALRM:
            receivedSIGALARM = true;
            alarm(REFRESH_RATE);
            break;
        default:
            break;
    }
}

void checkInputs(int args, char* argv[]) {

    if (args <= 2) {
        printf("Error: Failed to provide procnanny.server hostname and port.\n");
        exit(EXIT_FAILURE);
    }
    if (sscanf(argv[1], "%s", hostname) != 1) {
        printf("Error: Failed to read given port.\n");
        exit(EXIT_FAILURE);
    }
    if (sscanf(argv[2], "%d", &port) != 1) {
        printf("Error: Failed to read given hostname.\n");
        exit(EXIT_FAILURE);
    }
}

void killAllProcNannys() {
    pid_t pids[MAX_PROCESSES] = {-1};
    getPids("procnanny.client", pids);
    for(int i = 0; i < MAX_PROCESSES; i++) {
        if (pids[i] > 0 && pids[i] != getpid()) {
            if(kill(pids[i], 0) == 0) {
                killPid(pids[i]);
            }
        }
    }
}

void connectToServer() {
    struct sockaddr_in serverDetails;
    struct hostent *host;

    host = gethostbyname(hostname);

    if (host == NULL) {
        printf("Error: failed to resolve hostname.");
        exit(EXIT_FAILURE);
    }

    server = socket(AF_INET, SOCK_STREAM, 0);

    if (server < 0) {
        printf("Error: failed to intitialize server socket connection. ");
        exit(EXIT_FAILURE);
    }

    bzero((char *) &serverDetails, sizeof(serverDetails));

    serverDetails.sin_family = AF_INET;
    bcopy(host->h_addr, (char *) &serverDetails.sin_addr, (size_t) host->h_length);
    serverDetails.sin_port = htons((uint16_t) port);
    if (connect(server, (struct sockaddr *) &serverDetails, sizeof(serverDetails)) < 0) {
        printf("Error: failed to connect to server.");
        exit(EXIT_FAILURE);
    }
}

void readConfigurationFromServer(struct timeval * tv) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(server, &readable);
    int max_sd = server;

    int activity = select( max_sd + 1 , &readable , NULL , NULL , tv);

    if (FD_ISSET(server, &readable)) {
        for (int i = 0; i < CONFIG_FILE_LINES; i++) {
            configLines[i].runtime = 0;
            strcpy(configLines[i].programName, "");
        }

        char buff[2048];
        sleep(2);
        read(server, buff, 2048);
        int charsRead = 0;
        char program[64];
        unsigned int runtime;
        int extra;
        int i =0;
        while(0 < sscanf(buff + charsRead, "%s %d\n%n", program, &runtime, &extra)) {
            if (strcmp(program, "___KILL___") == 0) {
                cleanUp();
                exit(EXIT_SUCCESS);
            }
            strcpy(configLines[i].programName, program);
            configLines[i].runtime = runtime;
            i++;
            charsRead += extra;
        }
        printf("re-readconfig\n");
        fflush(stdout);
    }
}

void beginProcNanny() {
    ll_init(&monitoredProcesses, sizeof(MonitoredProcess), &monitoredProcessComparator);
    ll_init(&childProcesses, sizeof(ChildProcess), NULL);
    firstConfigurationReRead = false;
    checkForNewMonitoredProcesses(firstConfigurationReRead);
    alarm(REFRESH_RATE);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    while(true) {
        ll_forEach(&monitoredProcesses, &monitorNewProcesses);
        ll_forEach(&childProcesses, &checkChild);
        readConfigurationFromServer(&tv);

        if (receivedSIGALARM) {
            receivedSIGALARM = false;
            checkForNewMonitoredProcesses(firstConfigurationReRead);
            alarm(REFRESH_RATE);
        }
    }
}

void cleanUp() {
    ll_forEach(&childProcesses, &killChild);
    ll_free(&monitoredProcesses);
    ll_free(&childProcesses);
    close(server);
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



void killPid(pid_t pid) {
    char buff[256];
    snprintf(buff, 256, "kill -9 %d > /dev/null", pid);
    system(buff);
}

bool monitoredProcessComparator(void *mp1, void *mp2) {
    MonitoredProcess* first = (MonitoredProcess*) mp1;
    MonitoredProcess* second = (MonitoredProcess*) mp2;

    if (first->processPid == second->processPid) {
        return true;
    }

    return false;
}

void checkForNewMonitoredProcesses(bool logNoProcessesFound) {
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
                    ll_add_unique(&monitoredProcesses, &temp);
                    numberFound++;
                }
            }
//            if (logNoProcessesFound && numberFound == 0) {
//                LogMessage msg;
//                snprintf(msg.message, LOG_MESSAGE_LENGTH, "No '%s' processes found."
//                        , configLines[i].programName);
//                logToServer("Info", msg.message, false);
//            }
        }
    }

    firstConfigurationReRead = false;
}

void monitorNewProcesses(void *monitoredProcess) {
    MonitoredProcess* process = (MonitoredProcess*) monitoredProcess;
    if (process->beingMonitored == false) {
        ChildProcess* worker = ll_getIf(&childProcesses, &getChildPredicate);
        if (worker == NULL) {
            worker = spawnNewChildWorker();
        }
        initializeChild(worker, process);
        LogMessage msg;
        char hostname[256];
        gethostname(hostname, 256);
        snprintf(msg.message, LOG_MESSAGE_LENGTH, "Initializing monitoring of process '%s' (PID %d) on node %s.",
                 process->processName, process->processPid, hostname);
        logToServer("Info", msg.message, false);
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
            ll_free(&monitoredProcesses);
            ll_free(&childProcesses);
            close(server);
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

    // set to non blocking read of writes from child
    int flags = fcntl(worker.toParent.readWrite[READ_PIPE], F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(worker.toParent.readWrite[READ_PIPE], F_SETFL, flags);

    worker.childPid = forkResult;
    ll_add(&childProcesses, &worker);

    return (ChildProcess*) ll_getIf(&childProcesses, &getChildPredicate);
}

void checkChild(void *childProcess) {
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
            char hostname[256];
            gethostname(hostname, 256);
            snprintf(msg.message, LOG_MESSAGE_LENGTH, "PID %d (%s) on %s killed after exceeding %d seconds.",
                     child->processPid, hostname, child->processName, child->runtime);
            logToServer("Action", msg.message, false);
        }
        child->isAvailable = true;
        MonitoredProcess temp;
        temp.processPid = child->processPid;
        ll_remove(&monitoredProcesses, &temp);
    }
}

void killChild(void *childProcess) {
    ChildProcess* child = (ChildProcess*) childProcess;
    killPid(child->childPid);
}

void logToServer(const char *type, const char *msg, bool logToSTDOUT) {
    char timebuffer[TIME_BUFFER_SIZE];
    getCurrentTime(timebuffer);
    LogMessage logMsg;
    snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
             "[%s] %s: %s\n",
             timebuffer, type, msg);
    send(server, logMsg.message, LOG_MESSAGE_LENGTH, 0);
}


