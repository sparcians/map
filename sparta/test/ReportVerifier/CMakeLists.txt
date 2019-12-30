project(ReportVerifier_test)

add_executable(ReportVerifier_test ReportVerifier.cpp)

include(../TestingMacros.cmake)

sparta_test(ReportVerifier_test ReportVerifier_test_RUN)
