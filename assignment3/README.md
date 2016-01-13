#Assignment 3
Name: Kyle O'Shaughnessy
  
#About  
* `procnanny` represents a dumbed down "task manager" of sorts that is used to showcase UNIX Operating System concepts such as `fork`, `kill`, `pipe`, and sockets.  
* This iteration of `procnanny` has been split into the `procnanny.client` and `procnanny.server` programs.
* Given an configuration file as the first command line argument, `procnanny.sever` will manage connecting clients and send them the configuration. 
* Given the `procnanny.server` hostname and port number as the first two arguments, a remote or local `procnanny.client` will monitor all processes of the provided program names for the declared number of seconds and kill all remaining monitored processes after said amount of time.  
* A log file provided by the environment variable `PROCNANNYLOGS` will be appended to by `procnanny.server` with all info, actions,  errors, and warnings produced at runtime by both the client and the server.  
* A server info file provided by the environment variable `PROCNANNYSERVERINFO` will be written to with the `procnanny.server` hostname, pid, and port number.
* `procnanny.client` uses a forked child `procnanny.client` process for every qualified process found.  
  
#Compiling  
* To compile `procnanny.server` and `procnanny.client` , provide memwatch.c and memwatch.h in the same directory as this README (from http://www.linkdata.se/sourcecode/memwatch/) and simply run `make`.
* To clean the directory of all logs and binaries run `make clean`.  
  
#How to run  
* Create an configuration file with each line being a program name followed by a run time, `a.out 15` for example.
* Run `PROCNANNYLOGS="log_file_location" PROCNANNYSERVERINFO="server_info_location" ./procnanny.server inputFile.config`.
* If a user fails to set the `PROCNANNYLOGS` environment variable, a log will be created for them at `./procnanny.log`.  
* If a user fails to set the `PROCNANNYSERVERINFO` environment variable, a info will be created for them at `./procnanny.info`.
* If a user fails to provide a procnanny configuration file they will provided an appropriate error in the log. `procnanny` will also return with a code of 1.
* If there are any unrecoverable errors in the configuration file an error will be logged and `procnanny.sever` and all clients will cleanly exit with a return code of 1.

#Sources
* Basic and safe linked list operations bootstrap: `http://pseudomuto.com/development/2013/05/02/implementing-a-generic-linked-list-in-c/`, all  additions to the linked list code were implemented by myself.

