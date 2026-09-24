cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME stm32f103rbt6_a33)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})
option(A33_ENABLE_BOARD_DIAGNOSTICS "Enable Stage 2B board diagnostics" OFF)
option(A33_ENABLE_STAGE5I_USART3_BRINGUP
    "Enable Stage 5I-A USART3 raw bring-up diagnostics" OFF)
option(A33_ENABLE_STAGE5L_DIAGNOSTICS
    "Enable Stage 5L SWD-only measurement overrides" OFF)
option(A33_ENABLE_STAGE5MR5_BETA
    "Enable Stage 5M-R5 reference-lock engineering beta" OFF)
option(A33_ENABLE_STAGE5NA3_DIAGNOSTICS
    "Enable Stage 5N-A3 SWD-only invalid-input injection" OFF)
option(A33_ENABLE_STAGE5NB_BETA
    "Enable Stage 5N-B guarded ACTIVE alarm engineering beta" OFF)
option(A33_ENABLE_STAGE5MR5E_D1D_BETA
    "Enable Stage 5M-R5E-D1-D unified display engineering beta" OFF)
option(A33_ENABLE_STAGE5PA_PRODUCT
    "Enable Stage 5P-A controlled product candidate" OFF)
option(A33_ENABLE_STAGE5PA1_DIAGNOSTICS
    "Enable Stage 5P-A1 SWD-only throughput diagnostics" OFF)
option(A33_ENABLE_STAGE5PA1_CANDIDATE
    "Identify the Stage 5P-A1 engineering candidate as 0x0519" OFF)
option(A33_ENABLE_STAGE5PA1_MODBUS_OPTIMIZATION
    "Enable Stage 5P-A1 request-level Modbus read view" OFF)
option(A33_ENABLE_STAGE5PA2_PRODUCT
    "Enable Stage 5P-A2 engineering product candidate" OFF)
option(A33_ENABLE_STAGE5PA2C_PRODUCT
    "Enable Stage 5P-A2C local-menu transaction candidate" OFF)

if(A33_ENABLE_STAGE5NA3_DIAGNOSTICS AND NOT A33_ENABLE_STAGE5MR5_BETA)
    message(FATAL_ERROR "Stage 5N-A3 diagnostics require Stage 5M-R5 Beta")
endif()
if(A33_ENABLE_STAGE5NB_BETA AND NOT A33_ENABLE_STAGE5MR5_BETA)
    message(FATAL_ERROR "Stage 5N-B Beta requires Stage 5M-R5 Beta")
endif()
if(A33_ENABLE_STAGE5MR5E_D1D_BETA AND NOT A33_ENABLE_STAGE5NB_BETA)
    message(FATAL_ERROR "Stage 5M-R5E-D1-D Beta requires Stage 5N-B Beta")
endif()
if(A33_ENABLE_STAGE5PA_PRODUCT AND NOT A33_ENABLE_STAGE5MR5E_D1D_BETA)
    message(FATAL_ERROR "Stage 5P-A requires Stage 5M-R5E-D1-D")
endif()
if(A33_ENABLE_STAGE5PA1_DIAGNOSTICS AND NOT A33_ENABLE_STAGE5PA_PRODUCT)
    message(FATAL_ERROR "Stage 5P-A1 diagnostics require Stage 5P-A")
endif()
if(A33_ENABLE_STAGE5PA1_CANDIDATE AND NOT A33_ENABLE_STAGE5PA_PRODUCT)
    message(FATAL_ERROR "Stage 5P-A1 candidate requires Stage 5P-A")
endif()
if(A33_ENABLE_STAGE5PA2_PRODUCT AND NOT A33_ENABLE_STAGE5PA_PRODUCT)
    message(FATAL_ERROR "Stage 5P-A2 requires Stage 5P-A product")
endif()
if(A33_ENABLE_STAGE5PA2C_PRODUCT AND NOT A33_ENABLE_STAGE5PA2_PRODUCT)
    message(FATAL_ERROR "Stage 5P-A2C requires Stage 5P-A2 product")
endif()

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/calibration_controller.c
    App/communication_manager.c
    App/config_application.c
    App/app_main.c
    App/device_manager.c
    App/measurement_bridge.c
    App/metrology_manager.c
    App/persistence_manager.c
    App/revision_helper.c
    App/self_test_controller.c
    App/startup_auto_zero_controller.c
    App/system_context.c
    App/weighing_profile_manager.c
    BSP/bsp_adc.c
    BSP/bsp_board.c
    BSP/bsp_flash.c
    BSP/bsp_power_monitor.c
    BSP/bsp_rtu_timer.c
    BSP/bsp_gpio.c
    BSP/bsp_time.c
    BSP/bsp_uart.c
    BSP/bsp_uart_dma.c
    BSP/bsp_uart3_dma.c
    Config/alarm_config_validation.c
    Config/device_config_validator.c
    Config/default_config.c
    Diagnostics/stage2a_driver_diagnostics.c
    Diagnostics/stage2b_board_diagnostics.c
    Diagnostics/stage2b_display_font.c
    Diagnostics/stage3_metrology_diagnostics.c
    Diagnostics/stage4b_storage_diagnostics.c
    Diagnostics/stage5b_modbus_diagnostics.c
    Diagnostics/stage5c_ble_diagnostics.c
    Diagnostics/stage5i_usart3_diagnostics.c
    Domain/calibration/calibration_model.c
    Domain/calibration/raw_calibration_stability.c
    Domain/filter/weight_filter.c
    Domain/limit_checker/limit_checker.c
    Domain/measurement/metrology_config_validator.c
    Domain/measurement/display_conditioner.c
    Domain/measurement/mass_math.c
    Domain/measurement/metrology_standard_validator.c
    Domain/measurement/raw_measurement.c
    Domain/measurement/runtime_drift_compensator.c
    Domain/measurement/unit_converter.c
    Domain/measurement/weight_engine.c
    Domain/measurement/weight_math.c
    Domain/stability/stability_detector.c
    Domain/zero_tare/zero_tare.c
    Drivers/battery_adc/battery_adc.c
    Drivers/cs1237/cs1237.c
    Drivers/output_gpio/output_gpio.c
    Drivers/serial/rs485_tx_controller.c
    Drivers/serial/modbus_uart2_transport.c
    Drivers/serial/uart2_dma_position.c
    Drivers/serial/uart2_dma_transport.c
    Drivers/serial/uart3_modbus_transport.c
    Drivers/tm1628/tm1628.c
    Drivers/tm1628/tm1628_board_map.c
    Drivers/w02/w02_pwrkey.c
    Drivers/w02/w02_uart.c
    Protocol/command_service/command_service.c
    Protocol/crc16/crc16.c
    Protocol/ble/ble_frame_codec.c
    Protocol/ble/ble_checkweigh_codec.c
    Protocol/ble/ble_command_protocol.c
    Protocol/modbus/modbus_command_mailbox.c
    Protocol/modbus/modbus_register_model.c
    Protocol/modbus/modbus_crc16.c
    Protocol/modbus/modbus_rtu_timing.c
    Protocol/modbus/modbus_rtu_framer.c
    Protocol/modbus/modbus_rtu_server.c
    Protocol/modbus/modbus_rtu_transport.c
    Services/config_edit/config_edit.c
    Services/config_store/config_store.c
    Services/config_store/persistent_codec.c
    Services/crc32/crc32.c
    Services/event_queue/event_queue.c
    Services/fault_manager/fault_manager.c
    Services/scheduler/scheduler.c
    Services/storage_power_guard/storage_power_guard.c
    Services/ble_transport/ble_transport.c
    Services/ble_connection_manager/ble_connection_manager.c
    Services/ble_telemetry_service/ble_telemetry_scheduler.c
    Services/ble_telemetry_service/ble_telemetry_service.c
    Services/ble_command_service/ble_command_service.c
    UI/display_controller.c
    UI/display_model/display_codes.c
    UI/display_model/display_formatter.c
    UI/display_model/display_model.c
    UI/numeric_edit_cursor.c
    UI/key_service/key_map.c
    UI/key_service/key_service.c
    UI/menu_controller/menu_controller.c
    UI/status_controller/status_controller.c
    UI/status_controller/run_key_policy.c
)

if(A33_ENABLE_STAGE5L_DIAGNOSTICS)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Diagnostics/stage5l_measurement_diagnostics.c)
endif()

if(A33_ENABLE_STAGE5PA1_DIAGNOSTICS)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Diagnostics/stage5pa1_throughput_diagnostics.c)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA1_DIAGNOSTICS=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA1_DIAGNOSTICS=0)
endif()
if(A33_ENABLE_STAGE5PA1_CANDIDATE)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA1_CANDIDATE=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA1_CANDIDATE=0)
endif()
if(A33_ENABLE_STAGE5PA1_MODBUS_OPTIMIZATION)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA1_MODBUS_OPTIMIZATION=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA1_MODBUS_OPTIMIZATION=0)
endif()
if(A33_ENABLE_STAGE5PA2_PRODUCT)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA2_PRODUCT=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA2_PRODUCT=0)
endif()
if(A33_ENABLE_STAGE5PA2C_PRODUCT)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA2C_PRODUCT=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA2C_PRODUCT=0)
endif()

if(A33_ENABLE_STAGE5NA3_DIAGNOSTICS)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Diagnostics/stage5na3_fault_injection.c)
endif()

if(A33_ENABLE_STAGE5NB_BETA)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Services/guarded_checkweigh/guarded_checkweigh.c
        UI/checkweigh_local_control.c)
endif()

if(A33_ENABLE_STAGE5MR5E_D1D_BETA)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5MR5E_D1D_BETA=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5MR5E_D1D_BETA=0)
endif()
if(A33_ENABLE_STAGE5PA_PRODUCT)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA_PRODUCT=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5PA_PRODUCT=0)
endif()

if(A33_ENABLE_STAGE5MR5_BETA)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Domain/measurement/reference_lock_drift_compensator.c
        Experimental/stage5na/checkweigh_shadow.c
        UI/display_model/directional_display_follower.c
        UI/r5_local_control.c
        UI/ui_config_workspace.c)
endif()

if(A33_ENABLE_STAGE5NA3_DIAGNOSTICS)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5NA3_DIAGNOSTICS=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5NA3_DIAGNOSTICS=0)
endif()

if(A33_ENABLE_STAGE5MR5_BETA)
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        Experimental/stage5na)
endif()

if(NOT A33_ENABLE_BOARD_DIAGNOSTICS)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Services/alarm_output_manager/alarm_output_manager.c)
endif()

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App
    BSP
    Config
    Diagnostics
    Domain/calibration
    Domain/filter
    Domain/limit_checker
    Domain/measurement
    Domain/stability
    Domain/zero_tare
    Protocol/command_service
    Protocol/crc16
    Protocol/ble
    Protocol/modbus
    Drivers/battery_adc
    Drivers/cs1237
    Drivers/output_gpio
    Drivers/serial
    Drivers/tm1628
    Drivers/w02
    Services/ble_transport
    Services/ble_connection_manager
    Services/ble_telemetry_service
    Services/ble_command_service
    Services/alarm_output_manager
    Services/config_store
    Services/crc32
    Services/config_edit
    Services/event_queue
    Services/fault_manager
    Services/guarded_checkweigh
    Services/scheduler
    Services/storage_power_guard
    UI
    UI/display_model
    UI/key_service
    UI/menu_controller
    UI/status_controller
)

# Add project symbols (macros)
if(A33_ENABLE_STAGE5I_USART3_BRINGUP)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5I_USART3_BRINGUP=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5I_USART3_BRINGUP=0)
endif()

if(A33_ENABLE_STAGE5L_DIAGNOSTICS)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5L_DIAGNOSTICS=1
        STAGE5L_SWD_DIAGNOSTICS=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5L_DIAGNOSTICS=0
        STAGE5L_SWD_DIAGNOSTICS=0)
endif()

if(A33_ENABLE_STAGE5MR5_BETA)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5MR5_BETA=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5MR5_BETA=0)
endif()

if(A33_ENABLE_STAGE5NB_BETA)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5NB_BETA=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        A33_ENABLE_STAGE5NB_BETA=0)
endif()

if(A33_ENABLE_BOARD_DIAGNOSTICS)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        ENABLE_STAGE2B_BOARD_DIAGNOSTICS=1
        ENABLE_STAGE5E_A3_LOCAL_MENU=0)
    # Keep board diagnostics at -O0 while size-optimizing transport code that
    # the diagnostic image still links but does not need to single-step.
    set_source_files_properties(
        App/app_main.c
        App/communication_manager.c
        App/config_application.c
        App/persistence_manager.c
        App/system_context.c
        Drivers/serial/modbus_uart2_transport.c
        Drivers/serial/uart3_modbus_transport.c
        Protocol/command_service/command_service.c
        Protocol/modbus/modbus_register_model.c
        Protocol/modbus/modbus_rtu_framer.c
        Protocol/modbus/modbus_rtu_server.c
        Protocol/modbus/modbus_rtu_transport.c
        Services/ble_command_service/ble_command_service.c
        Services/ble_telemetry_service/ble_telemetry_service.c
        Services/config_edit/config_edit.c
        Services/config_store/config_store.c
        Services/config_store/persistent_codec.c
        UI/menu_controller/menu_controller.c
        UI/status_controller/run_key_policy.c
        UI/status_controller/status_controller.c
        PROPERTIES COMPILE_OPTIONS "-Os")
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        ENABLE_STAGE2B_BOARD_DIAGNOSTICS=0
        ENABLE_STAGE5E_A3_LOCAL_MENU=1)
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
