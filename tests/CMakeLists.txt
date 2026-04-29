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

add_executable(test_desk_controller_manual
    test_desk_controller_manual.c
    ../main/domain/desk_controller.c
)
target_include_directories(test_desk_controller_manual PRIVATE
    ../main/domain
)

add_test(NAME test_desk_controller_manual COMMAND test_desk_controller_manual)

add_executable(test_desk_controller_limits
    test_desk_controller_limits.c
    ../main/domain/desk_controller.c
)
target_include_directories(test_desk_controller_limits PRIVATE
    ../main/domain
)

add_test(NAME test_desk_controller_limits COMMAND test_desk_controller_limits)

add_executable(test_desk_controller_emergency
    test_desk_controller_emergency.c
    ../main/domain/desk_controller.c
)
target_include_directories(test_desk_controller_emergency PRIVATE
    ../main/domain
)

add_test(NAME test_desk_controller_emergency COMMAND test_desk_controller_emergency)

add_executable(test_desk_controller_deadtime
    test_desk_controller_deadtime.c
    ../main/domain/desk_controller.c
)
target_include_directories(test_desk_controller_deadtime PRIVATE
    ../main/domain
)

add_test(NAME test_desk_controller_deadtime COMMAND test_desk_controller_deadtime)
