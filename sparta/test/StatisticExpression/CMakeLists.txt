project(Statisticexpression_test)

sparta_add_test_executable(StatisticExpression_test StatisticExpression_test.cpp)

include(../TestingMacros.cmake)

sparta_test(StatisticExpression_test StatisticExpression_test_RUN)
