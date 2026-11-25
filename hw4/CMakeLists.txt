cmake_minimum_required(VERSION 3.10)
project(hw4_barbershop_tcp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

add_executable(barbershop_server
        src/server.c
        src/net_utils.c
)
add_executable(barbershop_barber
        src/barber.c
        src/net_utils.c
)
add_executable(barbershop_customer
        src/customer.c
        src/net_utils.c
)
add_executable(barbershop_observer
        src/observer.c
        src/net_utils.c
)