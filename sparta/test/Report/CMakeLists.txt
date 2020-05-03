project(Report_test)

sparta_add_executable(Report_test Report_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Report_test Report_test_RUN)
sparta_copy(Report_test *.EXPECTED)
sparta_copy(Report_test *.yaml)

add_subdirectory(Triggers)
