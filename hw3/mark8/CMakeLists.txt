cmake_minimum_required(VERSION 3.10)
project(hw3_mark8 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(barbershop_server
        src/server.c
        src/barbershop.c
        src/logger.c)

add_executable(barbershop_client
        src/client.c
        src/barbershop.c
        src/logger.c)

find_package(Threads REQUIRED)
target_link_libraries(barbershop_server PRIVATE Threads::Threads)
target_link_libraries(barbershop_client PRIVATE Threads::Threads)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(barbershop_server PRIVATE rt)
    target_link_libraries(barbershop_client PRIVATE rt)
endif()