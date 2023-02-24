project(Dag_test)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(DAG_test DAG_main.cpp)

sparta_test(DAG_test DAG_test_RUN)
