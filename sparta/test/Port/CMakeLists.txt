project(Port_test)

sparta_add_executable(Port_test Producer.cpp Consumer.cpp Port_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Port_test Port_test_RUN)
