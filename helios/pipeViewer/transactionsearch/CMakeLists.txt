project(TransactionSearch)

add_executable(transactionsearch src/transaction_search.cpp)

target_include_directories(transactionsearch PRIVATE ${CMAKE_SOURCE_DIR}/helios/pipeViewer)
target_link_libraries(transactionsearch ${Sparta_LIBS})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/transactionsearch TYPE BIN)
