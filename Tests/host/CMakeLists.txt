cmake_minimum_required(VERSION 3.22)
project(stage2b_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(stage2b_host_tests
    test_stage2a.c
    mocks.c
    ../../App/measurement_bridge.c
    ../../Diagnostics/stage2b_board_diagnostics.c
    ../../Diagnostics/stage2b_display_font.c
    ../../Domain/measurement/raw_measurement.c
    ../../Drivers/battery_adc/battery_adc.c
    ../../Drivers/cs1237/cs1237.c
    ../../Drivers/output_gpio/output_gpio.c
    ../../Drivers/tm1628/tm1628.c
    ../../Drivers/tm1628/tm1628_board_map.c
    ../../Drivers/w02/w02_pwrkey.c
)

target_compile_definitions(stage2b_host_tests PRIVATE
    STAGE2A_HOST_TEST=1
    ENABLE_STAGE2B_BOARD_DIAGNOSTICS=1
)
target_include_directories(stage2b_host_tests PRIVATE
    .
    ../../App
    ../../BSP
    ../../Config
    ../../Diagnostics
    ../../Domain/measurement
    ../../Drivers/battery_adc
    ../../Drivers/cs1237
    ../../Drivers/output_gpio
    ../../Drivers/tm1628
    ../../Drivers/w02
    ../../Services/event_queue
)

if(MSVC)
    target_compile_options(stage2b_host_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage2b_host_tests PRIVATE -Wall -Wextra -Werror)
endif()

enable_testing()
add_test(NAME stage2b_host_logic COMMAND stage2b_host_tests)
