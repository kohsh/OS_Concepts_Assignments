cmake_minimum_required(VERSION 3.3)
project(procnanny.server)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -DMEMWATCH -DMW_STDIO")

set(SOURCE_FILES_SERVER
    memwatch.c
    memwatch.h
    proc_nanny_server.c
    proc_nanny_server.h
    linked_list.h
    linked_list.c)

set(SOURCE_FILES_CLIENT
    memwatch.c
    memwatch.h
    proc_nanny_client.c
    proc_nanny_client.h
    linked_list.h
    linked_list.c)

add_executable(procnanny.server ${SOURCE_FILES_SERVER})

add_executable(procanny.client ${SOURCE_FILES_CLIENT})