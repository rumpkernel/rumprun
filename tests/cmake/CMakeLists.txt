cmake_minimum_required(VERSION 2.8.2)
project(Test C)

include(CheckIncludeFile)
include(CheckFunctionExists)

check_include_file(sys/epoll.h HAVE_SYS_EPOLL_H)
check_function_exists(select HAVE_SELECT)
check_function_exists(sigaction HAVE_SIGACTION)

configure_file(config.h.in config.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(cmake_test test.c)
