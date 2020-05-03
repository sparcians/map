project(Dag_test)

sparta_add_executable(DAG_test DAG_main.cpp)

include(../TestingMacros.cmake)

sparta_test(DAG_test DAG_test_RUN)
