project(Keyvalue_test)

add_executable(KeyValue_test KeyValue_test.cpp)

include(../TestingMacros.cmake)

sparta_test(KeyValue_test KeyValue_test_RUN)
