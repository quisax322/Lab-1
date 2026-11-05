cmake_minimum_required(VERSION 3.16)
project(ember CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX /EHsc /utf-8)
    add_link_options(/WX)
    # MSVC supports ASan, but not UBSan. /RTC1 is incompatible with ASan.
    option(EMBER_MSVC_ASAN "Use AddressSanitizer in MSVC Debug builds" ON)
    if(EMBER_MSVC_ASAN)
        string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
        add_compile_options($<$<CONFIG:Debug>:/Zi>)
        add_link_options($<$<CONFIG:Debug>:/INCREMENTAL:NO>)
        add_link_options($<$<CONFIG:Debug>:/DEBUG>)
        get_filename_component(compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        find_file(asan_runtime clang_rt.asan_dynamic-x86_64.dll
                  PATHS "${compiler_dir}" NO_DEFAULT_PATH)
        if(NOT asan_runtime)
            message(FATAL_ERROR "MSVC x64 ASan runtime missing; install ASan or set EMBER_MSVC_ASAN=OFF")
        endif()
    endif()
else()
    add_compile_options(-Wall -Wextra -Werror)
    add_compile_options($<$<CONFIG:Debug>:-fsanitize=address,undefined>
                        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>)
    add_link_options($<$<CONFIG:Debug>:-fsanitize=address,undefined>)
endif()

add_executable(ember
    src/main.cpp
    src/memory.cpp
    src/dump.cpp
    src/alu.cpp
    src/cpu.cpp
)

include(CTest)
if(BUILD_TESTING)
    add_executable(ember_tests tests/test_ember.cpp
                   src/alu.cpp src/cpu.cpp src/memory.cpp)
    target_include_directories(ember_tests PRIVATE src)
    add_test(NAME alu_cpu_memory COMMAND ember_tests)

    add_executable(lab03_tests tests/test_lab03.cpp src/alu.cpp src/cpu.cpp src/memory.cpp)
    target_include_directories(lab03_tests PRIVATE src)
    add_test(NAME lab03_memory_cpu COMMAND lab03_tests)

    add_executable(lab04_tests tests/test_lab04.cpp src/alu.cpp src/cpu.cpp src/memory.cpp)
    target_include_directories(lab04_tests PRIVATE src)
    add_test(NAME lab04_control_search COMMAND lab04_tests)

    foreach(experiment IN ITEMS assignment_experiment control_experiments scope_experiments)
        add_executable(${experiment} tests/${experiment}.cpp)
        add_test(NAME ${experiment}
            COMMAND ${CMAKE_COMMAND} -DEXPERIMENT=$<TARGET_FILE:${experiment}>
                -DCHECK=${experiment} -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_experiments.cmake)
    endforeach()
    # Only this isolated Notes experiment allows assignment-in-condition warnings.
    if(MSVC)
        target_compile_options(assignment_experiment PRIVATE /WX-)
        # The subject of this experiment is intentional local-name shadowing.
        target_compile_options(scope_experiments PRIVATE /wd4456)
    else()
        target_compile_options(assignment_experiment PRIVATE -Wno-error=parentheses)
    endif()

    add_executable(pointer_experiments tests/pointer_experiments.cpp)
    add_test(NAME pointer_experiments COMMAND pointer_experiments)

    add_executable(bounds_fixed tests/asan_oob.cpp)
    target_include_directories(bounds_fixed PRIVATE src)
    add_test(NAME asan_bounds_fixed COMMAND bounds_fixed)

    # Explicit opt-in target: not part of the normal build or CTest.
    add_executable(asan_oob EXCLUDE_FROM_ALL tests/asan_oob.cpp)
    target_include_directories(asan_oob PRIVATE src)
    target_compile_definitions(asan_oob PRIVATE EMBER_ASAN_DEMO_UNSAFE)

    foreach(check IN ITEMS lab-01 lab-02 lab-02-and lab-02-trace lab-02-input
                           lab-03 lab-03-h lab-03-trace lab-03-absolute lab-03-word lab-03-limit
                           lab-04 lab-04-trace lab-04-find lab-04-search-found lab-04-search-missing
                           lab-04-unknown lab-04-limit)
        add_test(NAME ${check}
            COMMAND ${CMAKE_COMMAND}
                -DEMBER=$<TARGET_FILE:ember>
                -DCHECK=${check}
                -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cli.cmake)
    endforeach()
endif()

if(MSVC AND EMBER_MSVC_ASAN)
    foreach(target IN ITEMS ember ember_tests lab03_tests pointer_experiments bounds_fixed asan_oob
                            lab04_tests assignment_experiment control_experiments scope_experiments)
        if(TARGET ${target})
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${asan_runtime}" "$<TARGET_FILE_DIR:${target}>"
                VERBATIM)
        endif()
    endforeach()
endif()
