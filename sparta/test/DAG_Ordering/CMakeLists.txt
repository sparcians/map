project(Dag_ordering)

sparta_add_executable(DAG_ordering DAG_Ordering_main.cpp)

include(../TestingMacros.cmake)

sparta_test(DAG_ordering DAG_ordering_RUN)
#sparta_copy(DAG_ordering gold.gexf)
#sparta_copy(DAG_ordering gold_cycle.gexf)
#sparta_assert_diff(DAG_ordering dag.gexf gold.gexf differs)
