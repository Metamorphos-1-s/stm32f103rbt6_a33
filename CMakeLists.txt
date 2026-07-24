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
    App/config_application.c
    App/app_main.c
    App/device_manager.c
    App/measurement_bridge.c
    App/metrology_manager.c
    App/persistence_manager.c
    App/self_test_controller.c
    App/system_context.c
    BSP/bsp_adc.c
    BSP/bsp_board.c
    BSP/bsp_flash.c
    BSP/bsp_power_monitor.c
    BSP/bsp_gpio.c
    BSP/bsp_time.c
    BSP/bsp_uart.c
    Config/default_config.c
    Diagnostics/stage2a_driver_diagnostics.c
    Diagnostics/stage2b_board_diagnostics.c
    Diagnostics/stage2b_display_font.c
    Diagnostics/stage3_metrology_diagnostics.c
    Diagnostics/stage4b_storage_diagnostics.c
    Domain/calibration/calibration_model.c
    Domain/calibration/raw_calibration_stability.c
    Domain/filter/weight_filter.c
    Domain/measurement/metrology_config_validator.c
    Domain/measurement/raw_measurement.c
    Domain/measurement/weight_engine.c
    Domain/measurement/weight_math.c
    Domain/stability/stability_detector.c
    Domain/zero_tare/zero_tare.c
    Drivers/battery_adc/battery_adc.c
    Drivers/cs1237/cs1237.c
    Drivers/output_gpio/output_gpio.c
    Drivers/tm1628/tm1628.c
    Drivers/tm1628/tm1628_board_map.c
    Drivers/w02/w02_pwrkey.c
    Protocol/command_service/command_service.c
    Services/config_edit/config_edit.c
    Services/config_store/config_store.c
    Services/config_store/persistent_codec.c
    Services/crc32/crc32.c
    Services/event_queue/event_queue.c
    Services/fault_manager/fault_manager.c
    Services/scheduler/scheduler.c
    Services/storage_power_guard/storage_power_guard.c
    UI/display_controller.c
    UI/display_model/display_codes.c
    UI/display_model/display_formatter.c
    UI/display_model/display_model.c
    UI/key_service/key_map.c
    UI/key_service/key_service.c
    UI/menu_controller/menu_controller.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App
    BSP
    Config
    Diagnostics
    Domain/calibration
    Domain/filter
    Domain/measurement
    Domain/stability
    Domain/zero_tare
    Protocol/command_service
    Drivers/battery_adc
    Drivers/cs1237
    Drivers/output_gpio
    Drivers/tm1628
    Drivers/w02
    Services/config_store
    Services/crc32
    Services/config_edit
    Services/event_queue
    Services/fault_manager
    Services/scheduler
    Services/storage_power_guard
    UI
    UI/display_model
    UI/key_service
    UI/menu_controller
)

# Add project symbols (macros)
if(A33_ENABLE_BOARD_DIAGNOSTICS)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        ENABLE_STAGE2B_BOARD_DIAGNOSTICS=1)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        ENABLE_STAGE2B_BOARD_DIAGNOSTICS=0)
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
