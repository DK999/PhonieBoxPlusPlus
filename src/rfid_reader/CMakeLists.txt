cmake_minimum_required(VERSION 3.10)
project(rfid_reader CXX)

# Find required packages for this component
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZEROMQ REQUIRED libzmq)

# Create executable
add_executable(rfid_reader
    main.cpp
    hid_reader/hid_rfid_reader.cpp
    hid_reader/hid_rfid_reader.h
    dummy_reader/dummy_rfid_reader.cpp
    dummy_reader/dummy_rfid_reader.h
)

# Link libraries
target_link_libraries(rfid_reader
    ${ZEROMQ_LIBRARIES}
    ${ZEROMQ_LDFLAGS}
    nlohmann_json::nlohmann_json
    ipc_commands_headers
    rt  # For input event handling
)

# Include directories
target_include_directories(rfid_reader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ZEROMQ_INCLUDE_DIRS}
)

# Set output name and destination
set_target_properties(rfid_reader PROPERTIES
    OUTPUT_NAME "rfid_reader"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/rfid_reader
)

install(TARGETS rfid_reader DESTINATION bin)