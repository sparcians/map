project(Keyvalue_test)

sparta_add_test_executable(KeyValue_test KeyValue_test.cpp)

include(../TestingMacros.cmake)

sparta_test(KeyValue_test KeyValue_test_RUN)
