project(TransactionSearch)

add_executable(transactionsearch src/transaction_search.cpp)
add_dependencies(transactionsearch sparta simdb)

target_link_libraries(transactionsearch ${Sparta_LIBS})
