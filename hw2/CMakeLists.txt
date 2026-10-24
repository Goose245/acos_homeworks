cmake_minimum_required(VERSION 3.20)
project(barbershop_threads C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")

find_package(Threads REQUIRED)

add_executable(barbershop_threads
        src/main.c
        src/logger.c
        src/barbershop.c
        src/types.h
        src/barbershop.h
)

target_link_libraries(barbershop_threads PRIVATE Threads::Threads)