cmake_minimum_required(VERSION 3.22)
project(stage4b_host_tests C)

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
    ../../App/weighing_profile_manager.c
    ../../Config/default_config.c
    ../../Diagnostics/stage2b_board_diagnostics.c
    ../../Diagnostics/stage2b_display_font.c
    ../../Diagnostics/stage3_metrology_diagnostics.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/calibration/raw_calibration_stability.c
    ../../Domain/filter/weight_filter.c
    ../../Domain/measurement/metrology_config_validator.c
    ../../Domain/measurement/mass_math.c
    ../../Domain/measurement/metrology_standard_validator.c
    ../../Domain/measurement/raw_measurement.c
    ../../Domain/measurement/unit_converter.c
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
    ../../Services/config_store
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

add_executable(stage4b_storage_tests
    test_stage4b.c
    fake_flash_backend.c
    ../../App/system_context.c
    ../../Config/default_config.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/measurement/metrology_config_validator.c
    ../../Domain/measurement/mass_math.c
    ../../Domain/measurement/metrology_standard_validator.c
    ../../Domain/measurement/unit_converter.c
    ../../Domain/measurement/weight_math.c
    ../../Services/config_store/config_store.c
    ../../Services/config_store/persistent_codec.c
    ../../Services/crc32/crc32.c
)
target_include_directories(stage4b_storage_tests PRIVATE
    . ../../App ../../Config ../../Domain/calibration ../../Domain/filter
    ../../Domain/measurement ../../Domain/stability
    ../../Services/config_store ../../Services/crc32
)
if(MSVC)
    target_compile_options(stage4b_storage_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage4b_storage_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME stage4b_storage_logic COMMAND stage4b_storage_tests)

add_executable(stage4br_persistence_tests
    test_persistence_manager.c
    persistence_test_adapters.c
    fake_flash_backend.c
    ../../App/persistence_manager.c
    ../../App/system_context.c
    ../../Config/default_config.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/measurement/metrology_config_validator.c
    ../../Domain/measurement/mass_math.c
    ../../Domain/measurement/metrology_standard_validator.c
    ../../Domain/measurement/unit_converter.c
    ../../Domain/measurement/weight_math.c
    ../../Services/config_store/config_store.c
    ../../Services/config_store/persistent_codec.c
    ../../Services/crc32/crc32.c
    ../../Services/storage_power_guard/storage_power_guard.c
)
target_compile_definitions(stage4br_persistence_tests PRIVATE
    ENABLE_STAGE2B_BOARD_DIAGNOSTICS=0)
target_include_directories(stage4br_persistence_tests PRIVATE
    . ../../App ../../BSP ../../Config ../../Diagnostics
    ../../Domain/calibration ../../Domain/filter ../../Domain/measurement
    ../../Domain/stability ../../Protocol/command_service
    ../../Services/config_store ../../Services/crc32 ../../Services/event_queue
    ../../Services/fault_manager ../../Services/storage_power_guard
    ../../UI ../../UI/display_model)
if(MSVC)
    target_compile_options(stage4br_persistence_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage4br_persistence_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME stage4br_persistence_logic COMMAND stage4br_persistence_tests)

add_executable(stage5a_host_tests
    test_stage5a.c
    stage5a_model_adapters.c
    ../../Config/default_config.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/measurement/mass_math.c
    ../../Domain/measurement/metrology_config_validator.c
    ../../Domain/measurement/metrology_standard_validator.c
    ../../Domain/measurement/unit_converter.c
    ../../Domain/measurement/weight_math.c
    ../../Services/config_store/persistent_codec.c
    ../../Protocol/modbus/modbus_command_mailbox.c
    ../../Protocol/modbus/modbus_register_model.c
    ../../UI/key_service/key_map.c
    ../../UI/key_service/key_service.c
)
target_include_directories(stage5a_host_tests PRIVATE
    . ../../App ../../Config ../../Domain/calibration ../../Domain/filter
    ../../Domain/measurement ../../Domain/stability ../../Services/config_store
    ../../Services/fault_manager ../../Services/storage_power_guard
    ../../Protocol/command_service ../../Protocol/modbus ../../Drivers/cs1237
    ../../UI/key_service)
if(MSVC)
    target_compile_options(stage5a_host_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage5a_host_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME stage5a_host_logic COMMAND stage5a_host_tests)
