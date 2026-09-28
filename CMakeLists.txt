cmake_minimum_required(VERSION 3.15)
project(PhonieBoxpp)

enable_testing()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable generation of compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(COMMAND_SCHEMA_JSON ${CMAKE_CURRENT_SOURCE_DIR}/config/ipc_command_schema.json)
set(COMMAND_HEADER_OUT  ${CMAKE_BINARY_DIR}/generated/ipc_commands.hpp)
set(GENERATOR_SCRIPT    ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_command_types.py)

add_custom_command(
    OUTPUT  ${COMMAND_HEADER_OUT}
    COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT}
            ${COMMAND_SCHEMA_JSON} ${COMMAND_HEADER_OUT}
    DEPENDS ${COMMAND_SCHEMA_JSON} ${GENERATOR_SCRIPT}
    COMMENT "Generating ipc_commands.hpp from ${COMMAND_SCHEMA_JSON}"
    VERBATIM
)

add_custom_target(generate_ipc_commands ALL
    DEPENDS ${COMMAND_HEADER_OUT}
)

# Add executable for the main system launcher
add_executable(rfid_system_launcher
    src/main.cpp
)

# Add include directories
target_include_directories(rfid_system_launcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(rfid_system_launcher PRIVATE
    nlohmann_json::nlohmann_json
    ipc_commands_headers
)

add_library(ipc_commands_headers INTERFACE)
target_include_directories(ipc_commands_headers INTERFACE ${CMAKE_BINARY_DIR}/generated)
add_dependencies(ipc_commands_headers generate_ipc_commands)

# Add subdirectories
add_subdirectory(src/media_controller)
add_subdirectory(src/rfid_parser)
add_subdirectory(src/rfid_reader)
add_subdirectory(src/volume_controller)
add_subdirectory(tests)




################################################################################
# Clangd: Copy compile_commands.json to source root after every build
################################################################################
add_custom_target(copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_BINARY_DIR}/compile_commands.json"
        "${PROJECT_SOURCE_DIR}/compile_commands.json"
    DEPENDS "${PROJECT_BINARY_DIR}/compile_commands.json"
    COMMENT "Copying compile_commands.json to source root for clangd"
    VERBATIM
)