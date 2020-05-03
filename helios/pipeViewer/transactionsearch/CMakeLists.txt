project(TransactionSearch)

add_executable(transactionsearch src/transaction_search.cpp)
add_dependencies(transactionsearch sparta simdb)

set(Boost_USE_STATIC_LIBS OFF)
set(BUILD_SHARED_LIBS ON)

find_package(Boost 1.49.0 REQUIRED COMPONENTS regex)
target_link_libraries(transactionsearch ${Sparta_LIBS} Boost::regex)
