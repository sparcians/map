project(MetaTypeList)

sparta_add_test_executable(MetaTypeList MetaTypeList_test.cpp)

include(../TestingMacros.cmake)

sparta_test(MetaTypeList MetaTypeList_RUN)


