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

#ifndef PROC_NANNY_H_
#define PROC_NANNY_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

#define MAX_PROCESSES 1024
#define CONFIG_FILE_LINES 256
#define LOG_MESSAGE_LENGTH 512
#define TIME_BUFFER_SIZE 40

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

__pid_t forkMonitorProcess(const char *process, unsigned int monitorTime);

void beginProcNanny(const char *configurationFile);
void checkInputs(int args, char* argv[]);
void exitError(const char* errorMessage);
void freeConfigLines();
void getCurrentTime(char* buffer);
void getPids(const char* processName, pid_t pids[MAX_PROCESSES]);
void killAllProcNannys();
void monitorProcess(const char *process, unsigned int monitorTime);
void readPipes();
void trimWhitespace(char* str);
void writeToPipe(Pipe* pPipe, const char* message);

#endif /* PROC_NANNY_H_ */
