project(Statisticexpression_test)

add_executable(StatisticExpression_test StatisticExpression_test.cpp)

include(../TestingMacros.cmake)

sparta_test(StatisticExpression_test StatisticExpression_test_RUN)
