// <BinaryIArchive> -*- C++ -*-

#ifndef __SPARTA_STATISTICS_BINARY_IARCHIVE_H__
#define __SPARTA_STATISTICS_BINARY_IARCHIVE_H__

#include "sparta/statistics/dispatch/archives/ArchiveSource.hpp"
#include "sparta/utils/SpartaAssert.hpp"

#include <fstream>

namespace sparta {
namespace statistics {

/*!
 * \brief Use a binary archive file as a source of
 * statistics values.
 */
class BinaryIArchive : public ArchiveSource
{
public:
    //One-time initialization. Open input files.
    void initialize() override {
        const std::string & path = getPath();
        const std::string & subpath = getSubpath();
        openBinaryArchiveFile_(path, subpath);
    }

    const std::vector<double> & readFromSource() override {
        //Pick an arbitrary buffer size to read. This can be
        //tuned for better overall performance, but 10000 values
        //at a time is reasonable.
        static const size_t BUF_LEN = 10000;
        static const size_t BUF_NUM_BYTES = BUF_LEN * sizeof(double);

        values_.resize(BUF_LEN);
        char * buffer = reinterpret_cast<char*>(&values_[0]);
        binary_fin_.read(buffer, BUF_NUM_BYTES);
        auto actual_bytes_read = binary_fin_.gcount();

        //We store statistics values as doubles on disk, so no
        //matter what BUF_LEN value is chosen, the number of bytes
        //read out must be a multiple of 8, or something went wrong.
        sparta_assert(actual_bytes_read % sizeof(double) == 0);

        values_.resize(actual_bytes_read / sizeof(double));
        return values_;
    }

private:
    void openBinaryArchiveFile_(const std::string & path,
                                const std::string & subpath)
    {
        const std::string filename = path + "/" + subpath + "/values.bin";
        binary_fin_.open(filename, std::ios::binary);
        if (!binary_fin_) {
            throw SpartaException(
                "Unable to open archive file for read: ") << filename;
        }
    }

    std::ifstream binary_fin_;
    std::vector<double> values_;
};

} // namespace statistics
} // namespace sparta

#endif
