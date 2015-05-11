cmake_minimum_required(VERSION 3.2)
project(turtle_shell)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -ansi -O4")

#add_custom_target(mytest COMMAND make -C ${mytest_SOURCE_DIR}
#                         CLION_EXE_DIR=${PROJECT_BINARY_DIR})

set(SOURCE_FILES main.h main.c)
add_executable(turtle ${SOURCE_FILES})