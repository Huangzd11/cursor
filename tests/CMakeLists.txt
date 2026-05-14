include(GoogleTest)

add_executable(dlt645_tests
    test_bcd_codec.cpp
    test_address.cpp
    test_frame.cpp
    test_frame_codec.cpp
    test_data_item.cpp
    test_frame_transceiver.cpp
    test_meter_reader.cpp
    test_config.cpp
    test_result_reporter.cpp
    test_scheduler.cpp
)

target_link_libraries(dlt645_tests PRIVATE
    dlt645_lib
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(dlt645_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

gtest_discover_tests(dlt645_tests)
