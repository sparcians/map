project(SYNCPORT_PIPELINE)

include (../ExamplesMacros.cmake)

add_executable(sparta_syncport
  src/main.cpp
  src/SyncPortSim.cpp
  src/Producer.cpp
  src/Consumer.cpp)

sparta_application(sparta_syncport)
#
##
## Testing
##
#include (../../test/TestingMacros.cmake)
#sparta_copy(sparta_syncport "test_final_cfg.sh")
#sparta_copy(sparta_syncport "test_arch_with_override.sh")
#
## build this guy as part of the regression
#sparta_regress(sparta_syncport)
#sparta_named_test(sparta_syncport_200000 sparta_syncport -r 200000)
#sparta_named_test(sparta_syncport_producers sparta_syncport -p top.consumer.params.num_producers 10 --show-tree)
#sparta_named_test(sparta_syncport_v sparta_syncport -v)
#sparta_named_test(sparta_syncport_files sparta_syncport file1 file2)
#sparta_named_test(sparta_syncport_final_cfg sparta_syncport -p top.producer0.params.test_param 1000 --write-final-config final)
## Notice the below tests don't use sparta macros because we are just invoking a shell
## script that does a few calls to sparta_syncport and some other unix commands
## to verify correctness. It'll still get picked up by make regress.
#add_test(NAME sparta_syncport_arch_with_override COMMAND ./test_arch_with_override.sh)
#add_test(NAME sparta_syncport_test_final_cfg COMMAND ./test_final_cfg.sh)
