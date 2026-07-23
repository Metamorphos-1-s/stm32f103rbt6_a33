cmake_minimum_required(VERSION 3.22)
project(stage3_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(stage3_host_tests
    test_stage2a.c
    test_stage3.c
    synthetic_scale_source.c
    mocks.c
    ../../App/measurement_bridge.c
    ../../App/metrology_manager.c
    ../../App/system_context.c
    ../../Config/default_config.c
    ../../Diagnostics/stage2b_board_diagnostics.c
    ../../Diagnostics/stage2b_display_font.c
    ../../Diagnostics/stage3_metrology_diagnostics.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/filter/weight_filter.c
    ../../Domain/measurement/metrology_config_validator.c
    ../../Domain/measurement/raw_measurement.c
    ../../Domain/measurement/weight_engine.c
    ../../Domain/measurement/weight_math.c
    ../../Domain/stability/stability_detector.c
    ../../Domain/zero_tare/zero_tare.c
    ../../Drivers/battery_adc/battery_adc.c
    ../../Drivers/cs1237/cs1237.c
    ../../Drivers/output_gpio/output_gpio.c
    ../../Drivers/tm1628/tm1628.c
    ../../Drivers/tm1628/tm1628_board_map.c
    ../../Drivers/w02/w02_pwrkey.c
    ../../Services/fault_manager/fault_manager.c
)

target_compile_definitions(stage3_host_tests PRIVATE
    STAGE2A_HOST_TEST=1
    ENABLE_STAGE2B_BOARD_DIAGNOSTICS=1
)
target_include_directories(stage3_host_tests PRIVATE
    .
    ../../App
    ../../BSP
    ../../Config
    ../../Diagnostics
    ../../Domain/calibration
    ../../Domain/filter
    ../../Domain/measurement
    ../../Domain/stability
    ../../Domain/zero_tare
    ../../Drivers/battery_adc
    ../../Drivers/cs1237
    ../../Drivers/output_gpio
    ../../Drivers/tm1628
    ../../Drivers/w02
    ../../Services/event_queue
    ../../Services/fault_manager
)

if(MSVC)
    target_compile_options(stage3_host_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage3_host_tests PRIVATE -Wall -Wextra -Werror)
endif()

enable_testing()
add_test(NAME stage3_host_logic COMMAND stage3_host_tests)
