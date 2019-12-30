project(Report_triggers_test)

add_executable(Report_triggers_test Report_triggers.cpp)

include(../../TestingMacros.cmake)

sparta_test(Report_triggers_test Report_triggers_test_RUN)
sparta_copy(Report_triggers_test *.yaml)
