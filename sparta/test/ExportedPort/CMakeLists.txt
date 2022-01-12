project(ExportedPort_test)

sparta_add_test_executable(ExportedPort_test ExportedPort_test.cpp)

include(../TestingMacros.cmake)

sparta_test(ExportedPort_test)
