# Find required packages for media controller
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZEROMQ REQUIRED libzmq)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Find libvlc
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBVLC REQUIRED libvlc)

# Create vlcclient library
add_library(vlcclient
    vlcclient/vlcclient.cpp
)

# Link libraries for vlcclient
target_link_libraries(vlcclient
    ${LIBVLC_LIBRARIES}
    ${LIBVLC_LDFLAGS}
)

# Include directories for vlcclient
target_include_directories(vlcclient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBVLC_INCLUDE_DIRS}
)

# Set output name and destination for vlcclient
set_target_properties(vlcclient PROPERTIES
    OUTPUT_NAME "vlcclient"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/media_controller
)

# Create executable
add_executable(
    media_controller
    main.cpp
    media_controller/media_controller.cpp
)

# Link libraries
target_link_libraries(media_controller
    ${ZEROMQ_LIBRARIES}
    ${ZEROMQ_LDFLAGS}
    ${JSONCPP_LIBRARIES}
    vlcclient
    ipc_commands_headers
)

# Include directories
target_include_directories(media_controller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/media_controller
    ${CMAKE_CURRENT_SOURCE_DIR}/vlcclient
    ${ZEROMQ_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

# Set output name and destination
set_target_properties(media_controller PROPERTIES
    OUTPUT_NAME "media_controller"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/media_controller
)

# Install targets
install(
    TARGETS media_controller
    RUNTIME DESTINATION ${INSTALL_BIN_DIR}
    COMPONENT media_controller
)
