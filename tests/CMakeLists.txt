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
    -Wconversion
    -Wsign-conversion
    -Wswitch-enum
    -Wundef
)

option(DESK_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(DESK_ENABLE_COVERAGE "Enable Clang source coverage instrumentation" OFF)

if(DESK_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(DESK_ENABLE_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "DESK_ENABLE_COVERAGE requires Clang")
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif()

enable_testing()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(
    NAME test_web_contract
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_web_contract.py
)
add_test(
    NAME test_build_secrets
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_build_secrets.py
)

find_program(DESK_NODE_EXECUTABLE node)
if(DESK_NODE_EXECUTABLE)
    add_test(
        NAME test_web_javascript_syntax
        COMMAND ${DESK_NODE_EXECUTABLE} --check
                ${CMAKE_CURRENT_SOURCE_DIR}/../main/web/app.js
    )
endif()
add_test(
    NAME test_web_ui_source
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_web_ui_source.py
)
add_test(
    NAME test_quality_badge
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_quality_badge.py
)
add_test(
    NAME test_coverage_gate
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_coverage_gate.py
)
add_test(
    NAME test_documentation_links
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_documentation_links.py
)

add_executable(test_position_scale
    test_position_scale.c
    ../main/domain/position_scale.c
)
target_include_directories(test_position_scale PRIVATE
    ../main/domain
)

add_test(NAME test_position_scale COMMAND test_position_scale)

add_executable(test_bts7960_sequence
    test_bts7960_sequence.c
    ../main/hal/bts7960_sequence.c
    ../main/domain/motor_axis.c
)
target_include_directories(test_bts7960_sequence PRIVATE
    ../main/hal
    ../main/domain
)

add_test(NAME test_bts7960_sequence COMMAND test_bts7960_sequence)

add_executable(test_command_arbiter
    test_command_arbiter.c
    ../main/app/desk_command_arbiter.c
)
target_include_directories(test_command_arbiter PRIVATE
    ../main/app
)

add_test(NAME test_command_arbiter COMMAND test_command_arbiter)

add_executable(test_preset_controller
    test_preset_controller.c
    ../main/app/desk_preset_controller.c
)
target_include_directories(test_preset_controller PRIVATE
    ../main/app
    ../main/domain
)

add_test(NAME test_preset_controller COMMAND test_preset_controller)

add_executable(test_preset_record
    test_preset_record.c
    ../main/network/desk_preset_record.c
    ../main/app/desk_preset_controller.c
    ../main/hal/desk_crc32.c
)
target_include_directories(test_preset_record PRIVATE
    ../main/network
    ../main/app
    ../main/domain
    ../main/hal
)

add_test(NAME test_preset_record COMMAND test_preset_record)

add_executable(test_boot_health
    test_boot_health.c
    ../main/app/desk_boot_health.c
    ../main/domain/motor_axis.c
)
target_include_directories(test_boot_health PRIVATE
    ../main/app
    ../main/diagnostics
    ../main/domain
    ../main/hal
)

add_test(NAME test_boot_health COMMAND test_boot_health)

add_executable(test_diagnostic_log
    test_diagnostic_log.c
    ../main/diagnostics/desk_diagnostic_log.c
    ../main/hal/desk_crc32.c
)
target_include_directories(test_diagnostic_log PRIVATE
    ../main/diagnostics
    ../main/domain
    ../main/hal
)

add_test(NAME test_diagnostic_log COMMAND test_diagnostic_log)

add_executable(test_control_protocol
    test_control_protocol.c
    ../main/network/desk_control_protocol.c
)
target_include_directories(test_control_protocol PRIVATE
    ../main/network
    ../main/app
)

add_test(NAME test_control_protocol COMMAND test_control_protocol)

add_executable(test_auth
    test_auth.c
    ../main/network/desk_auth.c
)
target_include_directories(test_auth PRIVATE
    ../main/network
)

add_test(NAME test_auth COMMAND test_auth)

add_executable(test_network_config
    test_network_config.c
    ../main/network/desk_network_config.c
)
target_include_directories(test_network_config PRIVATE
    ../main/network
    ../main/app
)

add_test(NAME test_network_config COMMAND test_network_config)

add_executable(test_form_codec
    test_form_codec.c
    ../main/network/desk_form_codec.c
)
target_include_directories(test_form_codec PRIVATE
    ../main/network
)

add_test(NAME test_form_codec COMMAND test_form_codec)

add_executable(test_http_security
    test_http_security.c
    ../main/network/desk_http_security.c
)
target_include_directories(test_http_security PRIVATE
    ../main/network
)

add_test(NAME test_http_security COMMAND test_http_security)

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
    ../main/app/desk_command_arbiter.c
    ../main/app/desk_input_filter.c
    ../main/app/desk_preset_controller.c
    ../main/domain/desk_controller.c
)
target_include_directories(test_desk_app PRIVATE
    .
    ../main/app
    ../main/diagnostics
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
