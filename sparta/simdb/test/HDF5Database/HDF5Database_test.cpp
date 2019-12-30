/*!
 * \file HDF5Database_test.cpp
 *
 * \brief Tests functionality of SimDB's HDF5 implementation
 */

#include "simdb/test/SimDBTester.h"

//Core database headers
#include "simdb/ObjectManager.h"
#include "simdb/ObjectRef.h"
#include "simdb/TableRef.h"
#include "simdb/Errors.h"

//HDF5-specific headers
#include "simdb/impl/hdf5/HDF5ConnProxy.h"

//Standard headers
#include <random>

#define DB_DIR "test_dbs"
#define MatrixDblNumElems 2
#define MatrixInt32NumRows 3
#define MatrixInt32NumCols 2

#define PRINT_ENTER_TEST                                                              \
  std::cout << std::endl;                                                             \
  std::cout << "*************************************************************"        \
            << "*** Beginning '" << __FUNCTION__ << "'"                               \
            << "*************************************************************"        \
            << std::endl;

#define CREATE_HDF5_SCHEMA(obj_mgr, schema)                                           \
    try {                                                                             \
        obj_mgr.disableWarningMessages();                                             \
        const bool success = obj_mgr.createDatabaseFromSchema(                        \
            schema, std::unique_ptr<simdb::DbConnProxy>(new simdb::HDF5ConnProxy));   \
        if (!success) {                                                               \
            throw;                                                                    \
        }                                                                             \
    } catch (...) {                                                                   \
        throw simdb::DBException("Could not create HDF5 schema");                     \
    }

std::mt19937 rng;

//! \brief Pick a random integral number
template <typename T>
typename std::enable_if<
    std::is_integral<T>::value,
T>::type
chooseRand()
{
    std::uniform_int_distribution<T> dist;
    return dist(rng);
}

//! \brief Pick a random floating-point number
template <typename T>
typename std::enable_if<
    std::is_floating_point<T>::value,
    T>::type
chooseRand()
{
    std::normal_distribution<T> dist(0, 1000);
    return dist(rng);
}

//! \brief Fixed-size struct full of all the supported
//! POD data types in HDF5 SimDB. The word "compound"
//! is seen throughout this file, and it means the same
//! thing as "struct" - Compound is what HDF5 calls
//! structured data types.
struct CompoundPOD {
    char ch;
    int8_t i1;
    uint8_t ui1;
    int16_t i2;
    uint16_t ui2;
    int32_t i4;
    uint32_t ui4;
    int64_t i8;
    uint64_t ui8;
    float flt;
    double dbl;
};

//! Create a randomized struct. Values are fed into
//! HDF5 record creation APIs, read back from disk,
//! and verified for accuracy.
CompoundPOD createRandomCompoundPOD()
{
    CompoundPOD comp;
    comp.ch  = chooseRand<char>();
    comp.i1  = chooseRand<int8_t>();
    comp.ui1 = chooseRand<uint8_t>();
    comp.i2  = chooseRand<int16_t>();
    comp.ui2 = chooseRand<uint16_t>();
    comp.i4  = chooseRand<int32_t>();
    comp.ui4 = chooseRand<uint32_t>();
    comp.i8  = chooseRand<int64_t>();
    comp.ui8 = chooseRand<uint64_t>();
    comp.flt = chooseRand<float>();
    comp.dbl = chooseRand<double>();
    return comp;
}

//! \brief Given an ObjectRef wrapping an HDF5 record
//! on disk, and the expected CompoundPOD values, compare
//! the record value for accuracy.
void verifyCompound(std::unique_ptr<simdb::ObjectRef> & row,
                    const CompoundPOD & comp)
{
    EXPECT_NOTEQUAL(row.get(), nullptr);
    if (!row) {
        return;
    }
    EXPECT_EQUAL(row->getPropertyChar("ch"),    comp.ch);
    EXPECT_EQUAL(row->getPropertyInt8("i1"),    comp.i1);
    EXPECT_EQUAL(row->getPropertyUInt8("ui1"),  comp.ui1);
    EXPECT_EQUAL(row->getPropertyInt16("i2"),   comp.i2);
    EXPECT_EQUAL(row->getPropertyUInt16("ui2"), comp.ui2);
    EXPECT_EQUAL(row->getPropertyInt32("i4"),   comp.i4);
    EXPECT_EQUAL(row->getPropertyUInt32("ui4"), comp.ui4);
    EXPECT_EQUAL(row->getPropertyInt64("i8"),   comp.i8);
    EXPECT_EQUAL(row->getPropertyUInt64("ui8"), comp.ui8);
    EXPECT_EQUAL(row->getPropertyFloat("flt"),  comp.flt);
    EXPECT_EQUAL(row->getPropertyDouble("dbl"), comp.dbl);
}

//! \brief Use the simdb::Table::addField() API to build
//! a fixed-size, compound data type table for testing
//!
//! \return Schema object containing a table suitable for
//! writing CompoundPOD structs to disk
simdb::Schema createSchemaForContiguousCompoundPOD(
    const simdb::CompressionType compression)
{
    using dt = simdb::ColumnDataType;

    simdb::Schema schema;

    schema.addTable("MyCompound", compression)
        .addField("ch",   dt::char_t,   FOFFSET(CompoundPOD,ch))
        .addField("i1",   dt::int8_t,   FOFFSET(CompoundPOD,i1))
        .addField("ui1",  dt::uint8_t,  FOFFSET(CompoundPOD,ui1))
        .addField("i2",   dt::int16_t,  FOFFSET(CompoundPOD,i2))
        .addField("ui2",  dt::uint16_t, FOFFSET(CompoundPOD,ui2))
        .addField("i4",   dt::int32_t,  FOFFSET(CompoundPOD,i4))
        .addField("ui4",  dt::uint32_t, FOFFSET(CompoundPOD,ui4))
        .addField("i8",   dt::int64_t,  FOFFSET(CompoundPOD,i8))
        .addField("ui8",  dt::uint64_t, FOFFSET(CompoundPOD,ui8))
        .addField("flt",  dt::float_t,  FOFFSET(CompoundPOD,flt))
        .addField("dbl",  dt::double_t, FOFFSET(CompoundPOD,dbl));

    return schema;
}

//! \brief Use the simdb::Table::addColumn() API to build
//! a fixed-size, compound data type table for testing
//!
//! \return Schema object containing a table suitable for
//! writing CompoundPOD structs to disk
simdb::Schema createSchemaForNonContiguousCompoundPOD(
    const simdb::CompressionType compression)
{
    using dt = simdb::ColumnDataType;

    simdb::Schema schema;

    schema.addTable("MyCompound", compression)
        .addColumn("ch",  dt::char_t)
        .addColumn("i1",  dt::int8_t)
        .addColumn("ui1", dt::uint8_t)
        .addColumn("i2",  dt::int16_t)
        .addColumn("ui2", dt::uint16_t)
        .addColumn("i4",  dt::int32_t)
        .addColumn("ui4", dt::uint32_t)
        .addColumn("i8",  dt::int64_t)
        .addColumn("ui8", dt::uint64_t)
        .addColumn("flt", dt::float_t)
        .addColumn("dbl", dt::double_t);

    return schema;
}

//! \brief Fixed-size struct full of all the supported
//! POD data types in HDF5 SimDB, including fields that
//! are non-scalar (but still fixed-size) POD's.
struct CompoundWithMatrixPOD
{
    char ch;
    int8_t i1;
    uint8_t ui1;
    int16_t i2;
    uint16_t ui2;
    int32_t i4;
    uint32_t ui4;
    int64_t i8;
    uint64_t ui8;
    float flt;
    double dbl;
    double dblmat[MatrixDblNumElems];
    int32_t i4mat[MatrixInt32NumRows][MatrixInt32NumCols];
};

//! Create a randomized struct. Values are fed into
//! HDF5 record creation APIs, read back from disk,
//! and verified for accuracy.
CompoundWithMatrixPOD createRandomCompoundWithMatrixPOD()
{
    CompoundWithMatrixPOD comp;
    comp.ch  = chooseRand<char>();
    comp.i1  = chooseRand<int8_t>();
    comp.ui1 = chooseRand<uint8_t>();
    comp.i2  = chooseRand<int16_t>();
    comp.ui2 = chooseRand<uint16_t>();
    comp.i4  = chooseRand<int32_t>();
    comp.ui4 = chooseRand<uint32_t>();
    comp.i8  = chooseRand<int64_t>();
    comp.ui8 = chooseRand<uint64_t>();
    comp.flt = chooseRand<float>();
    comp.dbl = chooseRand<double>();
    for (int i = 0; i < MatrixDblNumElems; ++i) {
        comp.dblmat[i] = 3.14 * rand();
    }
    for (int i = 0; i < MatrixInt32NumRows; ++i) {
        for (int j = 0; j < MatrixInt32NumCols; ++j) {
            comp.i4mat[i][j] = 1.5245 * rand();
        }
    }
    return comp;
}

//! \brief Given an ObjectRef wrapping an HDF5 record
//! on disk, and the expected CompoundPOD values, compare
//! the record value for accuracy.
void verifyCompoundMatrix(std::unique_ptr<simdb::ObjectRef> & row,
                          const CompoundWithMatrixPOD & comp)
{
    EXPECT_NOTEQUAL(row.get(), nullptr);
    if (!row) {
        return;
    }
    EXPECT_EQUAL(row->getPropertyChar("ch"),    comp.ch);
    EXPECT_EQUAL(row->getPropertyInt8("i1"),    comp.i1);
    EXPECT_EQUAL(row->getPropertyUInt8("ui1"),  comp.ui1);
    EXPECT_EQUAL(row->getPropertyInt16("i2"),   comp.i2);
    EXPECT_EQUAL(row->getPropertyUInt16("ui2"), comp.ui2);
    EXPECT_EQUAL(row->getPropertyInt32("i4"),   comp.i4);
    EXPECT_EQUAL(row->getPropertyUInt32("ui4"), comp.ui4);
    EXPECT_EQUAL(row->getPropertyInt64("i8"),   comp.i8);
    EXPECT_EQUAL(row->getPropertyUInt64("ui8"), comp.ui8);
    EXPECT_EQUAL(row->getPropertyFloat("flt"),  comp.flt);
    EXPECT_EQUAL(row->getPropertyDouble("dbl"), comp.dbl);

    //TODO: Matrix data answer checking. We'll need a more
    //user-friendly ObjectRef API for reading this data back
    //in from disk.
}

//! \brief Use the simdb::Table::addField() API to build
//! a fixed-size, compound data type table for testing
//!
//! \return Schema object containing a table suitable for
//! writing CompoundPOD structs to disk
simdb::Schema createSchemaForContiguousCompoundMatrixPOD()
{
    using dt = simdb::ColumnDataType;

    simdb::Schema schema;

    schema.addTable("MyCompound")
        .addField("ch",     dt::char_t,   FOFFSET(CompoundWithMatrixPOD,ch))
        .addField("i1",     dt::int8_t,   FOFFSET(CompoundWithMatrixPOD,i1))
        .addField("ui1",    dt::uint8_t,  FOFFSET(CompoundWithMatrixPOD,ui1))
        .addField("i2",     dt::int16_t,  FOFFSET(CompoundWithMatrixPOD,i2))
        .addField("ui2",    dt::uint16_t, FOFFSET(CompoundWithMatrixPOD,ui2))
        .addField("i4",     dt::int32_t,  FOFFSET(CompoundWithMatrixPOD,i4))
        .addField("ui4",    dt::uint32_t, FOFFSET(CompoundWithMatrixPOD,ui4))
        .addField("i8",     dt::int64_t,  FOFFSET(CompoundWithMatrixPOD,i8))
        .addField("ui8",    dt::uint64_t, FOFFSET(CompoundWithMatrixPOD,ui8))
        .addField("flt",    dt::float_t,  FOFFSET(CompoundWithMatrixPOD,flt))
        .addField("dbl",    dt::double_t, FOFFSET(CompoundWithMatrixPOD,dbl))
        .addField("dblmat", dt::double_t, FOFFSET(CompoundWithMatrixPOD,dblmat))
            ->setDimensions({MatrixDblNumElems})
        .addField("i4mat",  dt::int32_t,  FOFFSET(CompoundWithMatrixPOD,i4mat))
            ->setDimensions({MatrixInt32NumRows, MatrixInt32NumCols});

    return schema;
}

//! \brief Verify data accuracy in HDF5 database files when
//! using the Table::addColumn() API to build the schema,
//! and the TableRef::createObjectWithArgs() API to create
//! the records.
void testCompoundDataWritesWithArgs()
{
    PRINT_ENTER_TEST

    {
        //Test without compression enabled
        simdb::Schema schema = createSchemaForNonContiguousCompoundPOD(
            simdb::CompressionType::NONE);

        simdb::ObjectManager obj_mgr(DB_DIR);
        CREATE_HDF5_SCHEMA(obj_mgr, schema);

        auto ctable = obj_mgr.getTable("MyCompound");
        auto c1 = createRandomCompoundPOD();
        auto c2 = createRandomCompoundPOD();

        auto row1 = ctable->createObjectWithArgs(
            "ch", c1.ch, "i1", c1.i1, "ui1", c1.ui1,
            "i2", c1.i2, "ui2", c1.ui2, "i4", c1.i4,
            "ui4", c1.ui4, "i8", c1.i8, "ui8", c1.ui8,
            "flt", c1.flt, "dbl", c1.dbl);

        auto row2 = ctable->createObjectWithArgs(
            "ch", c2.ch, "i1", c2.i1, "ui1", c2.ui1,
            "i2", c2.i2, "ui2", c2.ui2, "i4", c2.i4,
            "ui4", c2.ui4, "i8", c2.i8, "ui8", c2.ui8,
            "flt", c2.flt, "dbl", c2.dbl);

        verifyCompound(row1, c1);
        verifyCompound(row2, c2);
    }

    {
        //Now test with compression enabled
        simdb::Schema schema = createSchemaForNonContiguousCompoundPOD(
            simdb::CompressionType::BEST_COMPRESSION_RATIO);

        simdb::ObjectManager obj_mgr(DB_DIR);
        CREATE_HDF5_SCHEMA(obj_mgr, schema);

        auto ctable = obj_mgr.getTable("MyCompound");
        auto c1 = createRandomCompoundPOD();
        auto c2 = createRandomCompoundPOD();

        auto row1 = ctable->createObjectWithArgs(
            "ch", c1.ch, "i1", c1.i1, "ui1", c1.ui1,
            "i2", c1.i2, "ui2", c1.ui2, "i4", c1.i4,
            "ui4", c1.ui4, "i8", c1.i8, "ui8", c1.ui8,
            "flt", c1.flt, "dbl", c1.dbl);

        auto row2 = ctable->createObjectWithArgs(
            "ch", c2.ch, "i1", c2.i1, "ui1", c2.ui1,
            "i2", c2.i2, "ui2", c2.ui2, "i4", c2.i4,
            "ui4", c2.ui4, "i8", c2.i8, "ui8", c2.ui8,
            "flt", c2.flt, "dbl", c2.dbl);

        verifyCompound(row1, c1);
        verifyCompound(row2, c2);
    }
}

//! \brief Verify data accuracy in HDF5 database files when
//! using the Table::addColumn() API to build the schema,
//! and the TableRef::createObjectWithVals() API to create
//! the records.
void testCompoundDataWritesWithVals()
{
    PRINT_ENTER_TEST

    {
        //Test without compression enabled
        simdb::Schema schema = createSchemaForNonContiguousCompoundPOD(
            simdb::CompressionType::NONE);

        simdb::ObjectManager obj_mgr(DB_DIR);
        CREATE_HDF5_SCHEMA(obj_mgr, schema);

        auto ctable = obj_mgr.getTable("MyCompound");
        auto c1 = createRandomCompoundPOD();
        auto c2 = createRandomCompoundPOD();

        auto row1 = ctable->createObjectWithVals(
            c1.ch, c1.i1, c1.ui1,
            c1.i2, c1.ui2, c1.i4,
            c1.ui4, c1.i8, c1.ui8,
            c1.flt, c1.dbl);

        auto row2 = ctable->createObjectWithVals(
            c2.ch, c2.i1, c2.ui1,
            c2.i2, c2.ui2, c2.i4,
            c2.ui4, c2.i8, c2.ui8,
            c2.flt, c2.dbl);

        verifyCompound(row1, c1);
        verifyCompound(row2, c2);

        //Now get a new TableRef tied to the same MyCompound
        //table. We should be able to get records from the
        //database file through either TableRef.
        ctable = obj_mgr.getTable("MyCompound");
        auto c3 = createRandomCompoundPOD();

        auto row3 = ctable->createObjectWithVals(
             c3.ch, c3.i1, c3.ui1,
             c3.i2, c3.ui2, c3.i4,
             c3.ui4, c3.i8, c3.ui8,
             c3.flt, c3.dbl);

        verifyCompound(row3, c3);
    }

    {
        //Now test with compression enabled
        simdb::Schema schema = createSchemaForNonContiguousCompoundPOD(
            simdb::CompressionType::BEST_COMPRESSION_RATIO);

        simdb::ObjectManager obj_mgr(DB_DIR);
        CREATE_HDF5_SCHEMA(obj_mgr, schema);

        auto ctable = obj_mgr.getTable("MyCompound");
        auto c1 = createRandomCompoundPOD();
        auto c2 = createRandomCompoundPOD();

        auto row1 = ctable->createObjectWithVals(
            c1.ch, c1.i1, c1.ui1,
            c1.i2, c1.ui2, c1.i4,
            c1.ui4, c1.i8, c1.ui8,
            c1.flt, c1.dbl);

        auto row2 = ctable->createObjectWithVals(
            c2.ch, c2.i1, c2.ui1,
            c2.i2, c2.ui2, c2.i4,
            c2.ui4, c2.i8, c2.ui8,
            c2.flt, c2.dbl);

        verifyCompound(row1, c1);
        verifyCompound(row2, c2);

        //Now get a new TableRef tied to the same MyCompound
        //table. We should be able to get records from the
        //database file through either TableRef.
        ctable = obj_mgr.getTable("MyCompound");
        auto c3 = createRandomCompoundPOD();

        auto row3 = ctable->createObjectWithVals(
             c3.ch, c3.i1, c3.ui1,
             c3.i2, c3.ui2, c3.i4,
             c3.ui4, c3.i8, c3.ui8,
             c3.flt, c3.dbl);

        verifyCompound(row3, c3);
    }
}

//! \brief Verify data accuracy in HDF5 database files when
//! using the Table::addField() API to build the schema, and
//! the TableRef::createObjectFromStruct() API to create the
//! records.
void testCompoundDataWritesFromStruct()
{
    PRINT_ENTER_TEST

    {
        //Test without compression enabled
        simdb::Schema schema = createSchemaForContiguousCompoundPOD(
            simdb::CompressionType::NONE);

        simdb::ObjectManager obj_mgr(DB_DIR);
        CREATE_HDF5_SCHEMA(obj_mgr, schema);

        auto ctable = obj_mgr.getTable("MyCompound");
        auto c1 = createRandomCompoundPOD();
        auto c2 = createRandomCompoundPOD();

        auto row1 = ctable->createObjectFromStruct(c1);
        auto row2 = ctable->createObjectFromStruct(c2);

        verifyCompound(row1, c1);
        verifyCompound(row2, c2);
    }

    {
        //Now test with compression enabled
        simdb::Schema schema = createSchemaForContiguousCompoundPOD(
            simdb::CompressionType::BEST_COMPRESSION_RATIO);

        simdb::ObjectManager obj_mgr(DB_DIR);
        CREATE_HDF5_SCHEMA(obj_mgr, schema);

        auto ctable = obj_mgr.getTable("MyCompound");
        auto c1 = createRandomCompoundPOD();
        auto c2 = createRandomCompoundPOD();

        auto row1 = ctable->createObjectFromStruct(c1);
        auto row2 = ctable->createObjectFromStruct(c2);

        verifyCompound(row1, c1);
        verifyCompound(row2, c2);
    }
}

//! \brief Verify data accuracy in HDF5 database files when
//! using the Table::addField() API to build the schema, and
//! the TableRef::createObjectFromStruct() API to create the
//! records.
void testCompoundMatrixDataWritesFromStruct()
{
    PRINT_ENTER_TEST

    simdb::Schema schema = createSchemaForContiguousCompoundMatrixPOD();
    simdb::ObjectManager obj_mgr(DB_DIR);
    CREATE_HDF5_SCHEMA(obj_mgr, schema);

    auto ctable = obj_mgr.getTable("MyCompound");
    auto c1 = createRandomCompoundWithMatrixPOD();
    auto c2 = createRandomCompoundWithMatrixPOD();

    auto row1 = ctable->createObjectFromStruct(c1);
    auto row2 = ctable->createObjectFromStruct(c2);

    verifyCompoundMatrix(row1, c1);
    verifyCompoundMatrix(row2, c2);
}

//! \brief Create a fixed-size HDF5 dataset, and attempt to
//! write records into it that are not the expected number
//! of bytes. Verify the exceptions are throw.
void testInvalidCompoundDataWritesWithVals()
{
    PRINT_ENTER_TEST

    //Test without compression enabled
    simdb::Schema schema = createSchemaForNonContiguousCompoundPOD(
        simdb::CompressionType::NONE);

    simdb::ObjectManager obj_mgr(DB_DIR);
    CREATE_HDF5_SCHEMA(obj_mgr, schema);

    auto ctable = obj_mgr.getTable("MyCompound");
    auto c1 = createRandomCompoundPOD();

    //Try to make an invalid call to createObjectWithArgs()
    //again. Start with too few arguments.
    EXPECT_THROW(
        ctable->createObjectWithVals(
            c1.ch, c1.i1, c1.ui1));

    //Try to make an invalid call to createObjectWithArgs()
    //again, this time calling with too many arguments.
    EXPECT_THROW(
        ctable->createObjectWithVals(
            c1.ch, c1.i1, c1.ui1,
            c1.i2, c1.ui2, c1.i4,
            c1.ui4, c1.i8, c1.ui8,
            c1.flt, c1.dbl, 1, 2, 3, 4, 5));
}

//! \brief Create an HDF5 database with some table records,
//! close the database and let the connection go out of scope,
//! then make a new connection to the same file and verify
//! the contents for accuracy.
void testDatabasePersistenceAcrossObjMgrs()
{
    PRINT_ENTER_TEST

    std::string db_file;
    CompoundPOD baseline_struct1;
    CompoundPOD baseline_struct2;
    simdb::DatabaseID db_id1;
    simdb::DatabaseID db_id2;

    {
        simdb::Schema schema = createSchemaForNonContiguousCompoundPOD(
            simdb::CompressionType::NONE);

        simdb::ObjectManager obj_mgr(DB_DIR);
        CREATE_HDF5_SCHEMA(obj_mgr, schema);
        db_file = obj_mgr.getDatabaseFile();

        auto ctable = obj_mgr.getTable("MyCompound");
        baseline_struct1 = createRandomCompoundPOD();
        baseline_struct2 = createRandomCompoundPOD();
        auto & c1 = baseline_struct1;
        auto & c2 = baseline_struct2;

        auto row1 = ctable->createObjectWithVals(
            c1.ch, c1.i1, c1.ui1,
            c1.i2, c1.ui2, c1.i4,
            c1.ui4, c1.i8, c1.ui8,
            c1.flt, c1.dbl);

        db_id1 = row1->getId();

        auto row2 = ctable->createObjectWithVals(
            c2.ch, c2.i1, c2.ui1,
            c2.i2, c2.ui2, c2.i4,
            c2.ui4, c2.i8, c2.ui8,
            c2.flt, c2.dbl);

        db_id2 = row2->getId();
    }

    {
        //The original ObjectManager / TableRef objects are gone,
        //but we have the database filename and the database IDs
        //of the records we just created. We should be able to
        //get those records back using brand new ObjectManager's.
        simdb::ObjectManager obj_mgr(".");
        EXPECT_TRUE(obj_mgr.connectToExistingDatabase(db_file));

        auto row1 = obj_mgr.findObject("MyCompound", db_id1);
        auto row2 = obj_mgr.findObject("MyCompound", db_id2);

        verifyCompound(row1, baseline_struct1);
        verifyCompound(row2, baseline_struct2);
    }
}

int main()
{
    rng.seed(time(0));

    testCompoundDataWritesWithArgs();
    testCompoundDataWritesWithVals();
    testInvalidCompoundDataWritesWithVals();
    testCompoundDataWritesFromStruct();
    testCompoundMatrixDataWritesFromStruct();
    testDatabasePersistenceAcrossObjMgrs();
}
