#Assignment 2  
Name: Kyle O'Shaughnessy
  
#About  
* `procnanny` represents a dumbed down "task manager" of sorts that is used to showcase UNIX Operating System concepts such as `fork`, `kill`, and `pipe`.  
* Given an configuration file as the first command line argument, procnanny will monitor all processes of the provided program names for the declared number of seconds and kill all remaining monitored processes after said amount of time.
* A log file provided by the environment variable `PROCNANNYLOGS` will be appended to by procnanny with all info, actions,  errors, and warnings produced at runtime.  
* `procnanny` uses a forked child `procnanny` process for every qualified process found. 
  
#Compiling  
* To compile `procnanny`, provide memwatch.c and memwatch.h in the same directoy as this README (from http://www.linkdata.se/sourcecode/memwatch/) and simply run `make`.
* To clean the directory of all logs and binaries run `make clean`.  
  
#How to run  
* Create an configuration file with each line being a program name followed by a run time, `a.out 15` for example.
* Run `PROCNANNYLOGS="log_file_location" ./procnanny inputFile.config`.
* If a user fails to set the `PROCNANNYLOGS` environment variable, a log will be created for them at `./procnanny.log`  
* If a user fails to provide a procnanny configuration file they will provided an appropriate error in the log. `procnanny` will also return with a code of 1.
* If there are any unrecoverable errors in the configuration file an error will be logged and `procnanny` will cleanly exit with a return code of 1.

#Sources
* Basic and safe linked list operations bootstrap: `http://pseudomuto.com/development/2013/05/02/implementing-a-generic-linked-list-in-c/`, all  additions to the linked list code were implemented by myself.

