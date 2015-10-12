cmake_minimum_required(VERSION 3.3)
project(ProcNanny)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -DMEMWATCH -DMW_STDIO")

set(SOURCE_FILES
    main.c
    memwatch.c
    memwatch.h
    proc_nanny.c
    proc_nanny.h
    linked_list.h
    linked_list.c)

add_executable(ProcNanny ${SOURCE_FILES})