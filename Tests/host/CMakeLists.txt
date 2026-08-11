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
    ../../Config/alarm_config_validation.c
    ../../Config/default_config.c
    ../../Diagnostics/stage2b_board_diagnostics.c
    ../../Diagnostics/stage2b_display_font.c
    ../../Diagnostics/stage3_metrology_diagnostics.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/calibration/raw_calibration_stability.c
    ../../Domain/filter/weight_filter.c
    ../../Domain/measurement/metrology_config_validator.c
    ../../Domain/measurement/display_conditioner.c
    ../../Domain/measurement/metrology_legacy_projection.c
    ../../Domain/measurement/mass_math.c
    ../../Domain/measurement/metrology_standard_validator.c
    ../../Domain/measurement/raw_measurement.c
    ../../Domain/measurement/runtime_drift_compensator.c
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

add_executable(display_conditioner_tests
    test_display_conditioner.c
    ../../Domain/measurement/display_conditioner.c)
target_include_directories(display_conditioner_tests PRIVATE
    ../../Domain/measurement)
if(MSVC)
    target_compile_options(display_conditioner_tests PRIVATE /W4 /WX)
else()
    target_compile_options(display_conditioner_tests PRIVATE
        -Wall -Wextra -Werror)
endif()
add_test(NAME display_conditioner_logic COMMAND display_conditioner_tests)

add_executable(runtime_drift_tests
    test_runtime_drift_compensator.c
    ../../Domain/measurement/mass_math.c
    ../../Domain/measurement/runtime_drift_compensator.c)
target_include_directories(runtime_drift_tests PRIVATE ../../Domain/measurement)
if(MSVC)
    target_compile_options(runtime_drift_tests PRIVATE /W4 /WX)
else()
    target_compile_options(runtime_drift_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME runtime_drift_logic COMMAND runtime_drift_tests)

add_executable(stage4b_storage_tests
    test_stage4b.c
    fake_flash_backend.c
    ../../App/system_context.c
    ../../Config/alarm_config_validation.c
    ../../Config/default_config.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/measurement/metrology_config_validator.c
    ../../Domain/measurement/metrology_legacy_projection.c
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
    ../../Config/alarm_config_validation.c
    ../../Config/default_config.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/measurement/metrology_config_validator.c
    ../../Domain/measurement/metrology_legacy_projection.c
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
    ../../Config/alarm_config_validation.c
    ../../Config/default_config.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/measurement/mass_math.c
    ../../Domain/measurement/runtime_drift_compensator.c
    ../../Domain/measurement/metrology_config_validator.c
    ../../Domain/measurement/metrology_legacy_projection.c
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
    ../../Domain/limit_checker ../../Services/alarm_output_manager
    ../../UI/key_service)
if(MSVC)
    target_compile_options(stage5a_host_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage5a_host_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME stage5a_host_logic COMMAND stage5a_host_tests)

add_executable(stage5b_host_tests
    test_stage5b.c
    stage5a_model_adapters.c
    stage5b_transport_adapters.c
    ../../App/communication_manager.c
    ../../Config/default_config.c
    ../../Domain/calibration/calibration_model.c
    ../../Domain/measurement/mass_math.c
    ../../Domain/measurement/runtime_drift_compensator.c
    ../../Domain/measurement/metrology_config_validator.c
    ../../Domain/measurement/metrology_legacy_projection.c
    ../../Domain/measurement/metrology_standard_validator.c
    ../../Domain/measurement/unit_converter.c
    ../../Domain/measurement/weight_math.c
    ../../Protocol/modbus/modbus_crc16.c
    ../../Protocol/crc16/crc16.c
    ../../Protocol/modbus/modbus_command_mailbox.c
    ../../Protocol/modbus/modbus_register_model.c
    ../../Protocol/modbus/modbus_rtu_server.c
    ../../Protocol/modbus/modbus_rtu_framer.c
    ../../Protocol/modbus/modbus_rtu_timing.c
    ../../Drivers/serial/rs485_tx_controller.c
    ../../Drivers/serial/uart2_dma_position.c
    ../../UI/key_service/key_service.c
    ../../UI/key_service/key_map.c
)
target_include_directories(stage5b_host_tests PRIVATE
    . ../../App ../../BSP ../../Config ../../Domain/calibration ../../Domain/filter
    ../../Domain/measurement ../../Domain/stability ../../Drivers/cs1237
    ../../Drivers/serial ../../Protocol/command_service ../../Protocol/modbus
    ../../Protocol/crc16
    ../../Services/config_store ../../Services/fault_manager
    ../../Services/storage_power_guard ../../Services/alarm_output_manager
    ../../Domain/limit_checker ../../UI/key_service)
if(MSVC)
    target_compile_options(stage5b_host_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage5b_host_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME stage5b_host_logic COMMAND stage5b_host_tests)

add_executable(stage5c_host_tests
    test_stage5c.c
    stage5c_transport_adapters.c
    ../../Services/ble_transport/ble_transport.c
    ../../Services/ble_connection_manager/ble_connection_manager.c
    ../../Services/event_queue/event_queue.c
    ../../Diagnostics/stage5c_ble_diagnostics.c
)
target_include_directories(stage5c_host_tests PRIVATE
    . ../../BSP ../../Config ../../Services/ble_transport
    ../../Services/ble_connection_manager ../../Services/event_queue
    ../../Drivers/w02 ../../Diagnostics)
if(MSVC)
    target_compile_options(stage5c_host_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage5c_host_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME stage5c_host_logic COMMAND stage5c_host_tests)

add_executable(stage5c_ble_telemetry_tests
    test_ble_telemetry.c
    ../../Protocol/crc16/crc16.c
    ../../Protocol/ble/ble_frame_codec.c
    ../../Protocol/ble/ble_checkweigh_codec.c
    ../../Services/ble_telemetry_service/ble_telemetry_scheduler.c
)
target_include_directories(stage5c_ble_telemetry_tests PRIVATE
    . ../../Protocol/crc16 ../../Protocol/ble
    ../../Services/ble_telemetry_service ../../Config ../../Domain/measurement
    ../../Domain/limit_checker)
if(MSVC)
    target_compile_options(stage5c_ble_telemetry_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage5c_ble_telemetry_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME stage5c_ble_telemetry_logic COMMAND stage5c_ble_telemetry_tests)

add_executable(stage5c_ble_command_protocol_tests
    test_ble_command_protocol.c
    ../../Protocol/crc16/crc16.c
    ../../Protocol/ble/ble_frame_codec.c
    ../../Protocol/ble/ble_command_protocol.c
)
target_include_directories(stage5c_ble_command_protocol_tests PRIVATE
    . ../../Protocol/crc16 ../../Protocol/ble)
if(MSVC)
    target_compile_options(stage5c_ble_command_protocol_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage5c_ble_command_protocol_tests PRIVATE
        -Wall -Wextra -Werror)
endif()
add_test(NAME stage5c_ble_command_protocol_logic
    COMMAND stage5c_ble_command_protocol_tests)

add_executable(stage5e_config_edit_no_menu_tests
    test_config_edit_no_menu.c
    ../../Services/config_edit/config_edit.c)
target_compile_definitions(stage5e_config_edit_no_menu_tests PRIVATE
    ENABLE_STAGE5E_A3_LOCAL_MENU=0)
target_include_directories(stage5e_config_edit_no_menu_tests PRIVATE
    ../../Config ../../Domain/calibration ../../Domain/measurement
    ../../Domain/stability ../../Domain/filter ../../Services/config_edit)
if(MSVC)
    target_compile_options(stage5e_config_edit_no_menu_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage5e_config_edit_no_menu_tests PRIVATE
        -Wall -Wextra -Werror)
endif()
add_test(NAME stage5e_config_edit_no_menu_logic
    COMMAND stage5e_config_edit_no_menu_tests)

add_executable(stage5c_ble_command_service_tests
    test_ble_command_service.c
    ../../Services/ble_command_service/ble_command_service.c
    ../../Protocol/ble/ble_command_protocol.c
    ../../Protocol/ble/ble_frame_codec.c
    ../../Protocol/crc16/crc16.c
)
target_include_directories(stage5c_ble_command_service_tests PRIVATE
    . ../../App ../../Config ../../Domain/measurement
    ../../Protocol/ble ../../Protocol/command_service ../../Protocol/modbus
    ../../Protocol/crc16
    ../../Services/ble_command_service ../../Services/ble_transport
    ../../Services/config_edit ../../Services/event_queue)
if(MSVC)
    target_compile_options(stage5c_ble_command_service_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage5c_ble_command_service_tests PRIVATE
        -Wall -Wextra -Werror)
endif()
add_test(NAME stage5c_ble_command_service_logic
    COMMAND stage5c_ble_command_service_tests)

add_executable(uart2_dma_recovery_tests
    test_uart2_dma_transport.c
    uart2_dma_transport_adapters.c
    ../../Drivers/serial/uart2_dma_transport.c
    ../../Drivers/serial/uart2_dma_position.c
)
target_include_directories(uart2_dma_recovery_tests PRIVATE
    . ../../BSP ../../Config ../../Drivers/serial ../../Domain/measurement)
if(MSVC)
    target_compile_options(uart2_dma_recovery_tests PRIVATE /W4 /WX)
else()
    target_compile_options(uart2_dma_recovery_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME uart2_dma_recovery_logic COMMAND uart2_dma_recovery_tests)

add_executable(checkweigh_tests
    test_checkweigh.c
    ../../Config/alarm_config_validation.c
    ../../Domain/limit_checker/limit_checker.c
    ../../Services/fault_manager/fault_manager.c
)
target_include_directories(checkweigh_tests PRIVATE
    ../../Config ../../Domain/limit_checker ../../Domain/measurement
    ../../Services/fault_manager)
if(MSVC)
    target_compile_options(checkweigh_tests PRIVATE /W4 /WX)
else()
    target_compile_options(checkweigh_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME checkweigh_logic COMMAND checkweigh_tests)

add_executable(alarm_output_manager_tests
    test_alarm_output_manager.c
    ../../Services/alarm_output_manager/alarm_output_manager.c
)
target_include_directories(alarm_output_manager_tests PRIVATE
    ../../Config ../../Domain/limit_checker ../../Domain/measurement
    ../../Drivers/output_gpio ../../Services/alarm_output_manager)
if(MSVC)
    target_compile_options(alarm_output_manager_tests PRIVATE /W4 /WX)
else()
    target_compile_options(alarm_output_manager_tests PRIVATE
        -Wall -Wextra -Werror)
endif()
add_test(NAME alarm_output_manager_logic COMMAND alarm_output_manager_tests)
