project(Scoreboard_test)

sparta_add_test_executable(Scoreboard_test Scoreboard_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Scoreboard_test Scoreboard_test_RUN)
