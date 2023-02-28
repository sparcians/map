project(TransactionSearch CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(transactionsearch src/transaction_search.cpp)

target_include_directories(transactionsearch PRIVATE ${CMAKE_SOURCE_DIR}/pipeViewer/pipe_view)
target_link_libraries(transactionsearch SPARTA::sparta)

install(TARGETS transactionsearch RUNTIME)
