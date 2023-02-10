project(Dag_test)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_add_test_executable(DAG_test DAG_main.cpp)

sparta_test(DAG_test DAG_test_RUN)
