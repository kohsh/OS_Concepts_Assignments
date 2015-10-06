#CMPUT 379 Assignment 1  
Name: Kyle O'Shaughnessy  
Student Number: 1369008  
Unix ID: koshaugh  
Lecture Section: EA1 Fa15  
Instructor: Paul Lu  
Lab Section: ED03  
TA: Marcus Karpoff  
  
#About  
* Given an input file as the first command line argument, procnanny will monitor all listed processes for the delcared number of seconds and kill them after said time.  
* A log file provided by the environment variable `PROCNANNYLOGS` will be appended to by procnanny with all info and errors produced during runtime.  
  
#Compiling  
* To compile procnanny provide memwatch.c and memwatch.h in the same directoy as this readme (from http://www.linkdata.se/sourcecode/memwatch/) and simply run `make`  
* To clean the directory of logs and binaries run `make clean`  
  
#How to run  
  
* Create an input file with the first line being the monitor time and all consecutive lines being monitored processes  
* Run `PROCNANNYLOGS="log_file_location"./procnanny inputFile.config`  
* If a user fails to set the PROCNANNYLOGS environment variable a log file will be created at `./procnanny.lg`  
* if a user fails to provide a procnanny configuration file they will provided an appropriate error in the log. procnanny will also return with a code of 1.  





