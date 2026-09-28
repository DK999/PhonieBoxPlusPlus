# Enable testing
enable_testing()

# Find required test packages
find_package(GTest REQUIRED)
# find_package(gmock REQUIRED)

# Create test executable for unit tests
add_executable(rfid_unit_tests
    unit/test_media_controller.cpp
    unit/test_volume_controller.cpp
    unit/test_config_parser.cpp
)

# Link libraries for unit tests
target_link_libraries(rfid_unit_tests
    ${ZEROMQ_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    volume_controller_lib
    # Add your actual library targets here
)

# Include directories for unit tests
target_include_directories(rfid_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/unit
    ${CMAKE_SOURCE_DIR}/
    ${CMAKE_SOURCE_DIR}/src/media_controller
    ${CMAKE_SOURCE_DIR}/src/media_controller/vlcclient
    ${CMAKE_SOURCE_DIR}/src/volume_controller
    ${ZEROMQ_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

# Create test executable for integration tests
add_executable(rfid_integration_tests
    integration/test_rfid_chain.cpp
    integration/test_integration_components.cpp
    integration/test_complete_system_flow.cpp
)

# Link libraries for integration tests
target_link_libraries(rfid_integration_tests
    ${ZEROMQ_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    # Add your actual library targets here
)

# Include directories for integration tests
target_include_directories(rfid_integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/
    ${CMAKE_SOURCE_DIR}/src/media_controller
    ${CMAKE_SOURCE_DIR}/src/media_controller/vlcclient
    ${CMAKE_SOURCE_DIR}/src/volume_controller
    ${CMAKE_CURRENT_SOURCE_DIR}/integration
    ${ZEROMQ_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

# Set C++ standard for tests
set_target_properties(rfid_unit_tests rfid_integration_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add tests to CTest
add_test(NAME rfid_unit_tests COMMAND rfid_unit_tests)
add_test(NAME rfid_integration_tests COMMAND rfid_integration_tests)