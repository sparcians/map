project(TransactionSearch)


if (COMPILE_32_MODE)
    message("Disabling transactionsearch build for 32 bit mode")
else()
    add_executable(transactionsearch src/transaction_search.cpp)
    add_dependencies(transactionsearch sparta simdb)
    target_link_libraries(transactionsearch ${REQUIRED_SPARTA_LIBS} $ENV{BOOST_LD_FLAGS})
    target_link_libraries(transactionsearch -L${PROJECT_BINARY_DIR}/../.. -L${PROJECT_BINARY_DIR}/../../simdb)
endif()
