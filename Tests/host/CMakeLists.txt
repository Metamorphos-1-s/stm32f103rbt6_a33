cmake_minimum_required(VERSION 3.22)
project(stage2a_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(stage2a_host_tests
    test_stage2a.c
    mocks.c
    ../../Drivers/battery_adc/battery_adc.c
    ../../Drivers/cs1237/cs1237.c
    ../../Drivers/tm1628/tm1628.c
    ../../Drivers/tm1628/tm1628_board_map.c
    ../../Drivers/w02/w02_pwrkey.c
)

target_compile_definitions(stage2a_host_tests PRIVATE STAGE2A_HOST_TEST=1)
target_include_directories(stage2a_host_tests PRIVATE
    .
    ../../BSP
    ../../Config
    ../../Drivers/battery_adc
    ../../Drivers/cs1237
    ../../Drivers/tm1628
    ../../Drivers/w02
    ../../Services/event_queue
)

if(MSVC)
    target_compile_options(stage2a_host_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage2a_host_tests PRIVATE -Wall -Wextra -Werror)
endif()

enable_testing()
add_test(NAME stage2a_host_logic COMMAND stage2a_host_tests)
