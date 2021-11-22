project(SKELETON_PIPELINE)

include (../ExamplesMacros.cmake)

add_executable(sparta_skeleton
  src/main.cpp
  src/SkeletonSimulator.cpp
  src/Producer.cpp
  src/Consumer.cpp)

sparta_application(sparta_skeleton)

#
# Testing
#
include (../../test/TestingMacros.cmake)
sparta_copy(sparta_skeleton "test_final_cfg.sh")
sparta_copy(sparta_skeleton "test_arch_with_override.sh")

# build this guy as part of the regression
sparta_regress(sparta_skeleton)
sparta_named_test(sparta_skeleton_200000 sparta_skeleton -r 200000)
sparta_named_test(sparta_skeleton_producers sparta_skeleton -p top.consumer.params.num_producers 10 --show-tree)
sparta_named_test(sparta_skeleton_v sparta_skeleton -v)
sparta_named_test(sparta_skeleton_files sparta_skeleton file1 file2)
sparta_named_test(sparta_skeleton_final_cfg sparta_skeleton -p top.producer0.params.test_param 1000 --write-final-config final)
# Notice the below tests don't use sparta macros because we are just invoking a shell
# script that does a few calls to sparta_skeleton and some other unix commands
# to verify correctness. It'll still get picked up by make regress.
add_test(NAME sparta_skeleton_arch_with_override COMMAND ./test_arch_with_override.sh)
add_test(NAME sparta_skeleton_test_final_cfg COMMAND ./test_final_cfg.sh)
