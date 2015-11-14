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

#ifndef PROC_NANNY_SERVER_H
#define PROC_NANNY_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <stdbool.h>

#define PORT 8888
#define MAXCLIENTS 32
#define MAX_PROCESSES 1024
#define CONFIG_FILE_LINES 256
#define LOG_MESSAGE_LENGTH 512
#define TIME_BUFFER_SIZE 40
#define PROGRAM_NAME_LENGTH 128

typedef struct _LogMessage {
    char message[LOG_MESSAGE_LENGTH];
} LogMessage;

typedef struct _ProgramConfig {
    char programName[PROGRAM_NAME_LENGTH];
    unsigned int runtime;
} ProgramConfig;

void beginProcNanny();
void checkInputs(int args, char* argv[]);
void cleanUp();
void getCurrentTime(char* buffer);
void getPids(const char* processName, pid_t pids[MAX_PROCESSES]);
void killPid(pid_t pid);
void killAllProcNannys();
void logToFileSimple(const char* msg);
void logToFile(const char* type, const char* msg, bool logToSTDOUT);
void readConfigurationFile();
void signalHandler(int signo);
void trimWhitespace(char* str);

#endif //PROC_NANNY_SERVER_H
