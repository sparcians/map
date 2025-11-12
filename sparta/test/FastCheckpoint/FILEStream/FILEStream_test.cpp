// <FILEStream_test> -*- C++ -*-

#include <inttypes.h>
#include <iostream>
#include <fstream>
#include <signal.h>

#include "sparta/serialization/checkpoint/FILEStream.hpp"
#include "sparta/utils/SpartaTester.hpp"

/*!
 * \file FILEStream_test.cpp
 * \brief Test for istream and ostream objects backed by FILE *
 */

TEST_INIT

std::string current_test;
std::string exception_str;
int signal_raised = 0;
void signalHandler(int signum)
{
    // Print out the failing test that caused the signal
    EXPECT_EQUAL(current_test, "");
    signal_raised = signum;
}

/*! Write stuff out to an ostream
 *
 * Used by multiple test cases.
 */
void writeStuffToOstream(std::ostream& os) {
    current_test = __FUNCTION__;

    // Write a few characters
    os << "a b c ";

    // Write a bunch of integers
    for (int i = 0; i < 1000; i++) {
        os << i << " ";
    }

    // One last integer
    os << 0xdeadbeef;

    // Write a string
    os.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit);
    EXPECT_NOTHROW(os.write("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.", 444));
}

/*! Read stuff from an istream and check it
 *
 * Used by multiple test cases.
 */
void readStuffFromIstreamAndCheck(std::istream& is) {
    current_test = __FUNCTION__;

    char c;
    is >> c;
    EXPECT_EQUAL('a', c);

    is >> c;
    EXPECT_EQUAL('b', c);

    is >> c;
    EXPECT_EQUAL('c', c);

    for (int i = 0; i < 1000; i++) {
        int j;
        is >> j;
        EXPECT_EQUAL(i, j);
    }

    uint32_t i;
    is >> i;
    EXPECT_EQUAL(0xdeadbeef, i);

    char buf[1000];
    memset(buf, 0, 1000);
    is.read(buf, 1000);
    EXPECT_EQUAL(is.gcount(), 444);
    EXPECT_TRUE(is.fail());
    EXPECT_TRUE(is.eof());
    std::string s(buf);

    EXPECT_EQUAL(s, "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum");
}

//! Test writing to a file stream backed by a file.
void testFileWrite() {
    current_test = __FUNCTION__;
    // Write some stuff out to a file backed by a FILE *.

    FILE *fd = fopen("testFile1", "w");
    EXPECT_NOTEQUAL(fd, nullptr);
    FILEOstream fos(fd);
    std::ostream& os = fos.getStream();
    writeStuffToOstream(os);
    fclose(fd);

    // Read it back using in ifstream, and check the contents.
    std::ifstream is("testFile1");
    readStuffFromIstreamAndCheck(is);
}

//! Test reading from a file stream backed by a file.
void testFileRead() {
    current_test = __FUNCTION__;
    // Write some stuff out to a file via an ofstream.
    std::ofstream os("testFile2");
    writeStuffToOstream(os);
    os.close();

    // Read it back using an istream tied to a FILE *, and check the contents.
    FILE *fd = fopen("testFile2", "r");
    EXPECT_NOTEQUAL(fd, nullptr);
    FILEIstream fis(fd);
    std::istream& is = fis.getStream();
    readStuffFromIstreamAndCheck(is);
    fclose(fd);
}

//! Test writing to a pipe through XZ, then reading back
void testXzCompressedFileWriteRead() {
    current_test = __FUNCTION__;
    FILE *pipeOut = popen("xz -6 - > testFile3.xz", "w");
    FILEOstream fos(pipeOut);
    std::ostream& os = fos.getStream();
    writeStuffToOstream(os);
    EXPECT_EQUAL(0, pclose(pipeOut));

    FILE *pipeIn = popen("xz -d -c testFile3.xz", "r");
    FILEIstream fis(pipeIn);
    std::istream& is = fis.getStream();
    readStuffFromIstreamAndCheck(is);
    EXPECT_EQUAL(0, pclose(pipeIn));
}

//! Test what happens when the FP is null
void testFpNull() {
    current_test = __FUNCTION__;
    EXPECT_THROW(FILEOstream fos(nullptr));
}

//! Test what happens when an output file isn't writable
void testFileNotWritable() {
    current_test = __FUNCTION__;
    FILE *fd = fopen("testFile3.xz", "r");  // Opened for reading
    FILEOstream fos(fd);
    std::ostream& os = fos.getStream();
    os.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit);
    EXPECT_THROW(os << "hello");
}

//! Test what happens when an invalid pipe command is provided
void testInvalidPipeCmd() {
    current_test = __FUNCTION__;
    FILE *pipeOut = popen("blah blah blah", "w");
    FILEOstream fos(pipeOut);
    std::ostream& os = fos.getStream();
    os.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit);
    EXPECT_NOTHROW(os << "hello");  // Doesn't throw...
    EXPECT_NOTEQUAL(0, pclose(pipeOut)); // Pipe command returns nonzero
}

int main() {
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGPIPE, signalHandler);

    try {
        testFileWrite();
        testFileRead();
        testXzCompressedFileWriteRead();
        testFpNull();
        testFileNotWritable();
        testInvalidPipeCmd();
    } catch (const sparta::SpartaException& ex) {
        exception_str = ex.what();
    }

    EXPECT_EQUAL(signal_raised, 0);
    EXPECT_EQUAL(exception_str, "");

    REPORT_ERROR;
    return ERROR_CODE;
}
