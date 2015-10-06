#! /bin/bash

./testproclaunch.sh&
PROCNANNYLOGS="./log.log" ./procnanny inputFile.txt&
