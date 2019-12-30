project(HierarchicalBuilding_test)

add_executable(HierarchicalBuilding_test HierarchicalBuilding_test.cpp)

include(../TestingMacros.cmake)

#sparta_test(HierarchicalBuilding_test HierarchicalBuilding_test_RUN)
#sparta_copy(HierarchicalBuilding_test *.yaml)

sparta_named_test(HierarchicalBuilding_test_1 HierarchicalBuilding_test)
sparta_named_test(HierarchicalBuilding_test_2 HierarchicalBuilding_test -p top.board_cfg.params.num_clusters 4 -p top.cluster2.params.num_cores 4 -p top.cluster*.params.core_type y)
