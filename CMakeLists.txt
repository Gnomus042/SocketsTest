cmake_minimum_required(VERSION 3.16)
project(sockets C)

set(CMAKE_C_STANDARD 11)

add_executable(sockets inet-server.c inet-client.c)