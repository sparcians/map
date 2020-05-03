project(Memorymap_test)

sparta_add_executable(MemoryMap_test MemoryMap_test.cpp)

include(../TestingMacros.cmake)

sparta_test(MemoryMap_test MemoryMap_test_RUN)
