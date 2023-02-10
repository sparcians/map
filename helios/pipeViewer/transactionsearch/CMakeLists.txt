project(TransactionSearch)

add_executable(transactionsearch src/transaction_search.cpp)
add_dependencies(transactionsearch sparta simdb)

target_include_directories(transactionsearch PRIVATE ${CMAKE_SOURCE_DIR}/helios/pipeViewer)
target_link_libraries(transactionsearch SPARTA::sparta)
