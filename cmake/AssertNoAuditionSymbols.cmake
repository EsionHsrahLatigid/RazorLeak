if(NOT DEFINED CXX_COMPILER)
    message(FATAL_ERROR "CXX_COMPILER is required")
endif()
if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "BINARY_DIR is required")
endif()

set(output_file "${BINARY_DIR}/hosted_preprocessed_RazorLeakEngine.ii")
execute_process(
    COMMAND "${CXX_COMPILER}" -std=c++20 -E -I "${SOURCE_DIR}/include" "${SOURCE_DIR}/tests/RazorLeakHostedCompileTests.cpp"
    RESULT_VARIABLE preprocess_result
    OUTPUT_FILE "${output_file}"
    ERROR_VARIABLE preprocess_error)

if(NOT preprocess_result EQUAL 0)
    message(FATAL_ERROR "Hosted preprocess failed: ${preprocess_error}")
endif()

file(READ "${output_file}" preprocessed)
string(FIND "${preprocessed}" "RazorLeakAuditionBridge" audition_symbol)
if(NOT audition_symbol EQUAL -1)
    message(FATAL_ERROR "Hosted macro-absent preprocessing still contains RazorLeakAuditionBridge")
endif()
