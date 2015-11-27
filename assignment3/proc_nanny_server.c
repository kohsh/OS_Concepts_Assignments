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
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include "proc_nanny_server.h"
#include "linked_list.h"
#include "memwatch.h"

bool receivedSIGHUP = false;
bool receivedSIGINT = false;

char logLocation[512];
char serverInfoLocation[512];
char configFileLocation[512];

int numProcessesKilled = 0;

int selfPipe[2];

ProgramConfig configLines[CONFIG_FILE_LINES];

int main(int args, char* argv[]) {

    if (signal(SIGHUP, &signalHandler) == SIG_ERR)
        printf("error with catching SIGHUP\n");

    if (signal(SIGINT, &signalHandler) == SIG_ERR)
        printf("error with catching SIGINT\n");


    checkInputs(args, argv);
    killAllProcNannys();
    sleep(1);
    readConfigurationFile();
    beginProcNanny();
    cleanUp();
    exit(EXIT_SUCCESS);
}

void signalHandler(int signo) {
    switch (signo) {
        case SIGINT:
            receivedSIGINT = true;
            break;
        case SIGHUP:
            receivedSIGHUP = true;
            break;
        default:
            break;
    }

    write(selfPipe[1], "x", 1);
}

void checkInputs(int args, char* argv[]) {
    char *procnannyLogs = getenv("PROCNANNYLOGS");
    char *procnannyServerInfo = getenv("PROCNANNYSERVERINFO");

    if (procnannyLogs == NULL) {
        snprintf(logLocation, 512, "./procnannyserver.log");
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
        snprintf(logLocation, 512, "%s", procnannyLogs);
    }

    if (procnannyServerInfo == NULL) {
        snprintf(serverInfoLocation, 512, "./procnannyserver.info");
        char timebuffer[TIME_BUFFER_SIZE];
        getCurrentTime(timebuffer);
        LogMessage logMsg;
        snprintf(logMsg.message, LOG_MESSAGE_LENGTH,
                 "[%s] Error: Environment variable 'PROCNANNYSERVERINFO' not specified.\n",
                 timebuffer);
        FILE* log = fopen(logLocation, "a");
        fprintf(log, "%s", logMsg.message);
        fclose(log);
    }
    else {
        snprintf(serverInfoLocation, 512, "%s", procnannyServerInfo);
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

void killAllProcNannys() {
    pid_t pids[MAX_PROCESSES] = {-1};
    getPids("procnanny.server", pids);
    for(int i = 0; i < MAX_PROCESSES; i++) {
        if (pids[i] > 0 && pids[i] != getpid()) {
            if(kill(pids[i], 0) == 0) {
                kill(pids[i], SIGINT);
            }
        }
    }
}

void readConfigurationFile() {
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    fp = fopen(configFileLocation, "r");
    if (fp == NULL) {
        logToFile("Error", "Could not read configuration file.", true);
        exit(EXIT_FAILURE);
    }

    int index = 0;

    for (int i = 0; i < CONFIG_FILE_LINES; i++) {
        configLines[i].runtime = 0;
        strcpy(configLines[i].programName, "");
    }

    while (getline(&line, &len, fp) != -1) {
        int numMatched = sscanf(line, "%s %d", configLines[index].programName, &configLines[index].runtime);
        if (numMatched == 1 || numMatched > 2) {
            LogMessage msg;
            snprintf(msg.message, LOG_MESSAGE_LENGTH,
                     "Expected two configuration arguments at line %d of %s.",
                     index, configFileLocation);
            logToFile("Error", msg.message, false);
            cleanUp();
            exit(EXIT_FAILURE);
        }
        if (strlen(configLines[index].programName) !=0) {
            trimWhitespace(configLines[index].programName);
        }
        index++;
        if (index >= CONFIG_FILE_LINES) {
            break;
        }
    }
    fclose(fp);
}

void beginProcNanny() {
    List clientNames;
    int serverSocket;
    int newSocket;
    int clientSockets[MAXCLIENTS] = {0};
    struct sockaddr_in server;
    int max_sd;

    fd_set readable;

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Create Socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Failed to create master socket");
        return;
    }

    // Bind
    if (bind(serverSocket, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("Failed to bind master socket");
        return;
    }

    // Listen
    if (listen(serverSocket, MAXCLIENTS) == -1) {
        perror("Failed to listen for connections");
        return;
    }

    // write server information to log and to stdout
    LogMessage msg;
    char name[64];
    gethostname(name, 64);
    snprintf(msg.message, LOG_MESSAGE_LENGTH, "PID %d on node %s, port %d", getpid(), name, PORT);
    logToFile("procnanny server", msg.message, false);

    // write server information to PROCNANNYSERVERINFO
    FILE* log = fopen(serverInfoLocation, "w");
    fprintf(log, "NODE %s PID %d PORT %d\n", name, getpid(), PORT);
    fclose(log);

    // setup self pipe and have no blocking
    pipe(selfPipe);
    int flags = fcntl(selfPipe[0], F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(selfPipe[0], F_SETFL, flags);
    flags = fcntl(selfPipe[1], F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(selfPipe[1], F_SETFL, flags);

    ll_init(&clientNames, sizeof(ClientName), NULL);

    // Accept
    while (1) {
        FD_ZERO(&readable);
        FD_SET(serverSocket, &readable);
        max_sd = serverSocket;

        FD_SET(selfPipe[0], &readable);
        if (selfPipe[0] > max_sd)
            max_sd = selfPipe[0];

        //add child sockets to set
        for (int i = 0 ; i < MAXCLIENTS ; i++)
        {
            int sd = clientSockets[i];
            if(sd > 0) {
                FD_SET(sd, &readable);
            }

            if(sd > max_sd) {
                max_sd = sd;
            }
        }

        int activity = select(max_sd + 1 , &readable , NULL , NULL , NULL);

        if (activity == -1) {
            continue;
        }

        // check the self pipe trick
        if (FD_ISSET(selfPipe[0], &readable)) {
            char ch;
            while(read(selfPipe[0], &ch, 1) != -1) {}

            if (receivedSIGHUP) {
                receivedSIGHUP = false;
                readConfigurationFile();

                for (int i = 0; i < MAXCLIENTS; i++) {
                    int sd = clientSockets[i];
                    if (sd == 0) {
                        continue;
                    }

                    for(int j = 0; j < CONFIG_FILE_LINES; j++) {
                        if (strlen(configLines[j].programName) != 0) {
                            char buffer[1024];
                            snprintf(buffer, 1024, "%s %d\n", configLines[j].programName, configLines[j].runtime);
                            send(newSocket, buffer, strlen(buffer), 0);
                        }
                    }
                }

                LogMessage msg;
                snprintf(msg.message, LOG_MESSAGE_LENGTH,
                         "Caught SIGHUP. Configuration file '%s' re-read.",
                         configFileLocation);
                char type[] = "Info";
                logToFile(type, msg.message, true);
            }

            if (receivedSIGINT) {
                receivedSIGINT = false;
                cleanUp();
                ll_free(&clientNames);
                for (int i = 0; i < MAXCLIENTS; i++) {
                    int sd = clientSockets[i];
                    char msg[] = "___KILL___ 0\n";
                    send(sd, msg, strlen(msg), 0);
                    close(sd);
                }
                LogMessage msg;
                snprintf(msg.message, LOG_MESSAGE_LENGTH,
                         "Caught SIGINT. Exiting cleanly. %d process(es) killed.",
                         numProcessesKilled);
                logToFile("Info", msg.message, true);
                close(selfPipe[0]);
                close(selfPipe[1]);
                close(serverSocket);
                exit(EXIT_SUCCESS);
            }
        }

        // Check for new connections
        else if (FD_ISSET(serverSocket, &readable)) {
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            if ((newSocket = accept(serverSocket, (struct sockaddr*)&client, &len)) < 0) {
                printf("Error with accept");
                exit(EXIT_FAILURE);
            }

            struct hostent* host = gethostbyaddr((const void*)&client.sin_addr, sizeof(struct in_addr), AF_INET);
            ClientName name;
            strncpy(name.name, host->h_name, strlen(host->h_name));
            ll_add(&clientNames, &name);

            //add new socket
            for (int i = 0; i < MAXCLIENTS; i++) {
                if( clientSockets[i] == 0 ) {
                    clientSockets[i] = newSocket;
                    break;
                }
            }

            // send the program configuration to the client
            for(int i = 0; i < CONFIG_FILE_LINES; i++) {
                if (strlen(configLines[i].programName) != 0) {
                    char buffer[1024];
                    snprintf(buffer, 1024, "%s %d\n", configLines[i].programName, configLines[i].runtime);
                    send(newSocket, buffer, strlen(buffer), 0);
                }
            }
        }

        else {
            //read data from the client
            for (int i = 0; i < MAXCLIENTS; i++) {
                int sd = clientSockets[i];
                ssize_t valread;
                char buffer[1024];

                if (FD_ISSET(sd, &readable)) {
                    // Check if client socket is closing
                    if ((valread = read(sd, buffer, 1024)) == 0) {
                        close(sd);
                        clientSockets[i] = 0;
                    }

                    // log the message
                    else {
                        buffer[valread] = '\0';
                        logToFileSimple(buffer);
                    }
                }
            }
        }
    }
}

void cleanUp() {
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
    snprintf(command, 511, "pidof %s", processName);
    FILE* pgrepOutput = popen(command, "r");

    char * line = NULL;
    size_t len = 0;
    if (pgrepOutput == NULL)
        return;

    int index = 0;
    if (getline(&line, &len, pgrepOutput) != -1) {
        char *pch = strtok(line, " ,.-");
        while (pch != NULL) {
            pids[index] = (pid_t) atoi(pch);
            pch = strtok(NULL, " ");
            index += 1;
        }
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

void logToFileSimple(const char* msg) {
    if (strstr(msg, "killed after") != NULL) {
        numProcessesKilled++;
    }

    FILE* log = fopen(logLocation, "a");
    fprintf(log, "%s", msg);
    fclose(log);
}
