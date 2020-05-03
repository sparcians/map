project(Peventhelper)

sparta_add_executable(PEventHelper PEventHelper_test.cpp)

include(../TestingMacros.cmake)

sparta_test(PEventHelper PEventHelper_RUN)
