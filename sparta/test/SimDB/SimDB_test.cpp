#include "simdb/sqlite/DatabaseManager.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

int main()
{
    // The point of this test is just to ensure that the git submodules
    // are setup correctly. Let's just write some code that at minimum
    // verifies we link against sqlite3 without issues.

    simdb::Schema schema;
    auto& table = schema.addTable("TheTable");

    using dt = simdb::SqlDataType;
    table.addColumn("TheInt", dt::int32_t);
    table.addColumn("TheString", dt::string_t);

    simdb::DatabaseManager db_mgr("test.db", true);
    db_mgr.appendSchema(schema);

    auto record = db_mgr.INSERT(
        SQL_TABLE("TheTable"),
        SQL_COLUMNS("TheInt", "TheString"),
        SQL_VALUES(112233, "HelloWorld"));

    EXPECT_EQUAL(record->getId(), 1);

    REPORT_ERROR;
    return ERROR_CODE;
}
