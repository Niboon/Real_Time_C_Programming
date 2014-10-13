cmake_minimum_required(VERSION 2.8.4)
project(Project_2_Mem_Alloc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    half_fit.c
    half_fit.h
    half_fit_test.c
    main.c)

add_executable(Project_2_Mem_Alloc ${SOURCE_FILES})