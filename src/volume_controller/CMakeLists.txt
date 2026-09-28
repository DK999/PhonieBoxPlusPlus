cmake_minimum_required(VERSION 3.10)
project(VolumeController LANGUAGES C CXX)

# Find required packages for volume control
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZEROMQ REQUIRED libzmq)

# Find ALSA and PulseAudio libraries
find_package(ALSA REQUIRED)
find_package(PulseAudio REQUIRED)

# Explicitly define PulseAudio libraries to ensure compatibility
set(PULSEAUDIO_LIBRARIES pulse pulse-simple rt)

# Find jsoncpp library
find_library(JSONCPP_LIBRARY NAMES jsoncpp REQUIRED)

# Create library for volume control
add_library(volume_controller_lib
    volume_backends/alsa_backend.cpp
    volume_backends/pulse_audio_backend.cpp
    volume_controller/volume_controller.cpp
)

# Link libraries
target_link_libraries(volume_controller_lib
    ${ZEROMQ_LIBRARIES}
    ${ZEROMQ_LDFLAGS}
    ${JSONCPP_LIBRARY}
    ${ALSA_LIBRARIES}
    ${PULSEAUDIO_LIBRARIES}
    ipc_commands_headers
)

# Include directories
target_include_directories(volume_controller_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ZEROMQ_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    ${PULSEAUDIO_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

# Create executable
add_executable(volume_controller
    main.cpp
    volume_controller/volume_controller.cpp
    volume_backends/alsa_backend.cpp
    volume_backends/pulse_audio_backend.cpp
)

# Link libraries
target_link_libraries(volume_controller
    ${ZEROMQ_LIBRARIES}
    ${ZEROMQ_LDFLAGS}
    ${JSONCPP_LIBRARY}
    ${ALSA_LIBRARIES}
    ${PULSEAUDIO_LIBRARIES}
    ipc_commands_headers
)

# Include directories
target_include_directories(volume_controller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ZEROMQ_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    ${PULSEAUDIO_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

# Set output name and destination
set_target_properties(volume_controller PROPERTIES
    OUTPUT_NAME "volume_controller"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/volume_controller
)

# Install target for volume_controller
install(TARGETS volume_controller
    RUNTIME DESTINATION ${INSTALL_BIN_DIR}
    COMPONENT volume_controller
)