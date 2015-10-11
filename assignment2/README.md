#CMPUT 379 Assignment 1  
Name: Kyle O'Shaughnessy  
Student Number: 1369008  
Unix ID: koshaugh  
Lecture Section: EA1 Fa15  
Instructor: Paul Lu  
Lab Section: ED03  
TA: Marcus Karpoff  
  
#About  
* `procnanny` represents a dumbed down "task manager" of sorts that is used to showcase UNIX Operating System concepts such as `fork`, `kill`, and `pipe`.   
* Given an input file as the first command line argument, procnanny will monitor all processes of the provided program names for the delcared number of seconds and kill all remaining monitored processes after said time.  
* A log file provided by the environment variable `PROCNANNYLOGS` will be appended to by procnanny with all info, actions,  errors, and warnings produced at runtime.  
* `procnanny` uses a forked child `procnanny` process for every program name provided. The child is then responsible for monitoring a discrete lifetime of all processes of that program existing on the system.  
  
#Compiling  
* To compile `procnanny` provide memwatch.c and memwatch.h in the same directoy as this readme (from http://www.linkdata.se/sourcecode/memwatch/) and simply run `make`  
* To clean the directory of all logs and binaries run `make clean`  
  
#How to run  
* Create an input file with the first line being the monitor time (numeric) and all consecutive lines being monitored programs to monitor  
* Run `PROCNANNYLOGS="log_file_location"./procnanny inputFile.config`  
* If a user fails to set the `PROCNANNYLOGS` environment variable, a log will be created for them at `./procnanny.log`  
* If a user fails to provide a procnanny configuration file they will provided an appropriate error in the log. `procnanny` will also return with a code of 1.  
