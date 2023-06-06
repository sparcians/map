project(Port_test)

sparta_add_test_executable(Port_test Producer.cpp Consumer.cpp Port_test.cpp)

sparta_test(Port_test Port_test_RUN)
