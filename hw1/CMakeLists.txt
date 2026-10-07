cmake_minimum_required(VERSION 3.16)
project(barbershop C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")

include_directories(src)

file(GLOB SOURCES "src/*.c")

add_executable(barbershop ${SOURCES}
        src/types.h
        src/logger.h
        src/logger.c
        src/event_queue.h
        src/event_queue.c
        src/barbershop.h
        src/Barbershop.c)
