project(TransactionDatabaseAPI)


set( TransactionDatabase_scripts ${PROJECT_SOURCE_DIR}/scripts/) 
set ( TransactionFile ${PROJECT_SOURCE_DIR}/data_test_0)




##################################################################
# Build the TransactionDatabaseAPI test command
##################################################################
add_custom_command(OUTPUT TransactionDatabaseAPI_test.sh
                   COMMAND printf 'export TRANSACTION_MODULE_DIR="${PROJECT_BINARY_DIR}/../../python/transactiondb/lib/" &&  export TRANSACTION_FILE="${TransactionFile}" &&  &&  python ${TransactionDatabase_scripts}/test_transactiondb.py' > TransactionDatabaseAPI_test.sh && chmod +x TransactionDatabaseAPI_test.sh
                   DEPENDS 
                   ${TransactionDatabase_scripts}/test_transactiondb.py
                   COMMENT ${TransactionDatabaseAPI_test_cmd})


add_custom_target (TransactionDatabaseAPI_test_sh_driver ALL DEPENDS TransactionDatabaseAPI_test.sh transactiondb2_driver
                   transactiondb_driver)

add_custom_command(TARGET TransactionDatabaseAPI_test
                   COMMAND ./TransactionDatabaseAPI_test.sh
                   DEPENDS 
                   TransactionDatabaseAPI_test.sh
                   )



# We need the python scripts to run currectly. 
#sparta_test(TransactionDatabaseAPI_test)
add_test(NAME TransactionDatabaseAPI_test COMMAND 
              TransactionDatabaseAPI_test.sh
              )

sparta_regress(TransactionDatabaseAPI_test_sh_driver)

# We need the pipeViewer files copied over. 
sparta_copy(TransactionDatabaseAPI_test_sh_driver data_test_0* .)
add_subdirectory(cpp_interface)
