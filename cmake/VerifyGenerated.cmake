foreach(required_variable IN ITEMS
    PROTOC_EXECUTABLE
    PLUGIN_EXECUTABLE
    GENERATOR_NAME
    PROTO_INCLUDE
    PROTO_FILE
    OUTPUT_DIR
    GENERATED_FILE_1
    EXPECTED_FILE_1
)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(generator_command
    "${PROTOC_EXECUTABLE}"
    "-I${PROTO_INCLUDE}"
    "--plugin=protoc-gen-${GENERATOR_NAME}=${PLUGIN_EXECUTABLE}"
)
if(DEFINED GENERATOR_OPTION AND NOT "${GENERATOR_OPTION}" STREQUAL "")
    list(APPEND generator_command "--${GENERATOR_NAME}_opt=${GENERATOR_OPTION}")
endif()
list(APPEND generator_command "--${GENERATOR_NAME}_out=${OUTPUT_DIR}" "${PROTO_FILE}")

execute_process(
    COMMAND ${generator_command}
    RESULT_VARIABLE generator_result
    OUTPUT_VARIABLE generator_stdout
    ERROR_VARIABLE generator_stderr
)
if(NOT generator_result EQUAL 0)
    message(FATAL_ERROR
        "protoc-gen-${GENERATOR_NAME} failed with ${generator_result}\n"
        "stdout:\n${generator_stdout}\n"
        "stderr:\n${generator_stderr}"
    )
endif()

foreach(index RANGE 1 2)
    if(DEFINED GENERATED_FILE_${index} OR DEFINED EXPECTED_FILE_${index})
        if(NOT DEFINED GENERATED_FILE_${index} OR NOT DEFINED EXPECTED_FILE_${index})
            message(FATAL_ERROR "generated/expected file ${index} must be specified together")
        endif()
        set(generated_path "${OUTPUT_DIR}/${GENERATED_FILE_${index}}")
        set(expected_path "${EXPECTED_FILE_${index}}")
        if(NOT EXISTS "${generated_path}")
            message(FATAL_ERROR "generator did not create ${generated_path}")
        endif()

        file(READ "${generated_path}" generated_content)
        file(READ "${expected_path}" expected_content)
        string(REPLACE "\r\n" "\n" generated_content "${generated_content}")
        string(REPLACE "\r\n" "\n" expected_content "${expected_content}")
        if(NOT generated_content STREQUAL expected_content)
            message(FATAL_ERROR
                "${generated_path} differs from ${expected_path}; regenerate the fixture"
            )
        endif()
    endif()
endforeach()
