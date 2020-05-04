project(Subjectstate_test)

sparta_add_test_executable(SubjectState_test SubjectState_main.cpp)

include(../TestingMacros.cmake)

sparta_test(SubjectState_test SubjectState_test_RUN)
