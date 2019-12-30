project(SpartaSharedPointer_test)

add_executable(SpartaSharedPointer_test SpartaSharedPointer_test.cpp)

include(../TestingMacros.cmake)

sparta_test(SpartaSharedPointer_test SpartaSharedPointer_test_RUN)
