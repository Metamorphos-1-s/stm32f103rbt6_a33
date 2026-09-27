cmake_minimum_required(VERSION 3.22)
project(stage5pa13c_replay_runner C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_executable(stage5pa13c_replay_runner
    replay_runner.c
    ../../../Domain/measurement/a13c_shadow_compensator.c)
target_include_directories(stage5pa13c_replay_runner PRIVATE
    ../../../Domain/measurement)
if(MSVC)
    target_compile_options(stage5pa13c_replay_runner PRIVATE /W4 /WX)
else()
    target_compile_options(stage5pa13c_replay_runner PRIVATE
        -Wall -Wextra -Werror)
endif()
add_executable(stage5pa13c_safety_tests
    test_safety.c
    ../../../Domain/measurement/a13c_shadow_compensator.c)
target_include_directories(stage5pa13c_safety_tests PRIVATE
    ../../../Domain/measurement)
if(MSVC)
    target_compile_options(stage5pa13c_safety_tests PRIVATE /W4 /WX)
else()
    target_compile_options(stage5pa13c_safety_tests PRIVATE
        -Wall -Wextra -Werror)
endif()
enable_testing()
add_test(NAME stage5pa13c_safety COMMAND stage5pa13c_safety_tests)
add_executable(stage5pa13c_host_benchmark
    benchmark.c
    ../../../Domain/measurement/a13c_shadow_compensator.c)
target_include_directories(stage5pa13c_host_benchmark PRIVATE
    ../../../Domain/measurement)
