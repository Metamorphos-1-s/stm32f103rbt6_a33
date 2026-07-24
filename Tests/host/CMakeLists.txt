cmake_minimum_required(VERSION 3.22)
project(stage4a_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(stage4a_host_tests
    test_stage2a.c
    test_stage3.c
    test_stage4a.c
    synthetic_scale_source.c
    mocks.c
    ../../App/measurement_bridge.c
    ../../App/calibration_controller.c
    ../../App/config_application.c
    ../../App/metrology_manager.c
    ../../App/self_test_controller.c
    ../../App/system_context.c
    ../../Config/default_config.c
    ../../Diagnostics/stage2b_board_diagnostics.c
    ../../Diagnostics/stage2b_display_font.c
    ../../Diagnostics/stage3_metrology_diagnostics.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/calibration/raw_calibration_stability.c
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
    ../../Protocol/command_service/command_service.c
    ../../Services/config_edit/config_edit.c
    ../../Services/fault_manager/fault_manager.c
    ../../UI/display_controller.c
    ../../UI/display_model/display_codes.c
    ../../UI/display_model/display_formatter.c
    ../../UI/display_model/display_model.c
    ../../UI/key_service/key_map.c
    ../../UI/key_service/key_service.c
    ../../UI/menu_controller/menu_controller.c
)

target_compile_definitions(stage4a_host_tests PRIVATE
    STAGE2A_HOST_TEST=1
    ENABLE_STAGE2B_BOARD_DIAGNOSTICS=1
)
target_include_directories(stage4a_host_tests PRIVATE
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
    ../../Protocol/command_service
    ../../Drivers/battery_adc
    ../../Drivers/cs1237
    ../../Drivers/output_gpio
    ../../Drivers/tm1628
    ../../Drivers/w02
    ../../Services/event_queue
    ../../Services/config_edit
    ../../Services/fault_manager
    ../../UI
    ../../UI/display_model
    ../../UI/key_service
    ../../UI/menu_controller
)

if(MSVC)
    target_compile_options(stage4a_host_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage4a_host_tests PRIVATE -Wall -Wextra -Werror)
endif()

enable_testing()
add_test(NAME stage4a_host_logic COMMAND stage4a_host_tests)
