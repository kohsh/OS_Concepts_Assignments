cmake_minimum_required(VERSION 3.3)
project(procnanny.client)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -DMEMWATCH -DMW_STDIO")

set(SOURCE_FILES
    memwatch.c
    memwatch.h
    proc_nanny_client.c
    proc_nanny_client.h
    linked_list.h
    linked_list.c)

add_executable(procnanny.client ${SOURCE_FILES})