project(Report_test)

add_executable(Report_test Report_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Report_test Report_test_RUN)
sparta_copy(Report_test *.EXPECTED)
sparta_copy(Report_test *.yaml)

add_subdirectory(Triggers)
