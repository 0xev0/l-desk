cmake_minimum_required(VERSION 3.22)

project(motorized_desk_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

add_executable(test_motor_axis
    test_motor_axis.c
    ../main/domain/motor_axis.c
)
target_include_directories(test_motor_axis PRIVATE
    ../main/domain
)

add_test(NAME test_motor_axis COMMAND test_motor_axis)
