# Find required packages for this component
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZEROMQ REQUIRED libzmq)

# Create executable
add_executable(rfid_parser 
    main.cpp
    rfid_parser.cpp
)

# Link libraries
target_link_libraries(rfid_parser 
    ${ZEROMQ_LIBRARIES}
    ${ZEROMQ_LDFLAGS}
    ${JSONCPP_LIBRARIES}
    ipc_commands_headers
)

# Include directories
target_include_directories(rfid_parser PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${JSONCPP_INCLUDE_DIRS}
    ${ZEROMQ_INCLUDE_DIRS}
)

# Set output name and destination
set_target_properties(rfid_parser PROPERTIES
    OUTPUT_NAME "rfid_parser"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/rfid_parser
)

install(TARGETS rfid_parser DESTINATION bin)