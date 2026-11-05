# Run manually after explicitly building asan_oob in Debug with ASan.
# cmake -DDEMO=.../asan_oob.exe -DREPORT=.../asan-report.txt -P tests/capture_asan.cmake
if(NOT DEFINED DEMO OR NOT DEFINED REPORT)
    message(FATAL_ERROR "Provide DEMO and REPORT")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "ASAN_OPTIONS=abort_on_error=0:symbolize=1" "${DEMO}"
    OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE result TIMEOUT 20)
file(WRITE "${REPORT}" "${output}${errors}")
file(WRITE "${REPORT}.exit.txt" "${result}\n")
if(result STREQUAL "0" OR NOT "${output}${errors}" MATCHES "AddressSanitizer: stack-buffer-overflow")
    message(FATAL_ERROR "Expected actual ASan stack-buffer-overflow; exit=${result}. See ${REPORT}")
endif()
message(STATUS "Captured actual ASan stack-buffer-overflow; exit=${result}")
