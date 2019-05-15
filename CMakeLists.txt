cmake_minimum_required(VERSION 3.14)
project(ex4_try C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(ex4_try osqueue.c osqueue.h threadPool.c threadPool.h test.c)