cmake_minimum_required(VERSION 3.10)
project(hw3_mark5 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(barbershop_processes
        src/main.c
        src/barbershop.c
        src/barbershop.h
        src/logger.c
        src/logger.h
        src/types.h)

find_package(Threads REQUIRED)
target_link_libraries(barbershop_processes PRIVATE Threads::Threads)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(barbershop_processes PRIVATE rt)
endif()