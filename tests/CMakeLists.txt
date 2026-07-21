cmake_minimum_required(VERSION 3.22)

project(motorized_desk_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(
    -Wall
    -Wextra
    -Werror
    -Wpedantic
    -Wformat=2
    -Wshadow
)

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

add_executable(test_desk_controller_alignment
    test_desk_controller_alignment.c
    ../main/domain/desk_controller.c
)
target_include_directories(test_desk_controller_alignment PRIVATE
    ../main/domain
)

add_test(NAME test_desk_controller_alignment COMMAND test_desk_controller_alignment)

add_executable(test_desk_controller_calibration
    test_desk_controller_calibration.c
    ../main/domain/desk_controller.c
)
target_include_directories(test_desk_controller_calibration PRIVATE
    ../main/domain
)

add_test(NAME test_desk_controller_calibration COMMAND test_desk_controller_calibration)

add_executable(test_desk_controller_edge_cases
    test_desk_controller_edge_cases.c
    ../main/domain/desk_controller.c
)
target_include_directories(test_desk_controller_edge_cases PRIVATE
    ../main/domain
)

add_test(NAME test_desk_controller_edge_cases COMMAND test_desk_controller_edge_cases)

add_executable(test_desk_input_filter
    test_desk_input_filter.c
    ../main/app/desk_input_filter.c
)
target_include_directories(test_desk_input_filter PRIVATE
    ../main/app
    ../main/domain
)

add_test(NAME test_desk_input_filter COMMAND test_desk_input_filter)

add_executable(test_desk_app
    test_desk_app.c
    fake_platform.c
    ../main/app/desk_app.c
    ../main/app/desk_input_filter.c
    ../main/domain/desk_controller.c
)
target_include_directories(test_desk_app PRIVATE
    .
    ../main/app
    ../main/domain
    ../main/hal
)

add_test(NAME test_desk_app COMMAND test_desk_app)

add_executable(test_safety_properties
    test_safety_properties.c
    ../main/domain/desk_controller.c
    ../main/domain/motor_axis.c
)
target_include_directories(test_safety_properties PRIVATE
    ../main/domain
)

add_test(NAME test_safety_properties COMMAND test_safety_properties)
