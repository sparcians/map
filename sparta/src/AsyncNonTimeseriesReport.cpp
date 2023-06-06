// <AsyncNonTimeseriesReport> -*- C++ -*-

#include "sparta/async/AsyncNonTimeseriesReport.hpp"

#include <cstdint>

//SQLite-specific headers
#include <zlib.h>

namespace sparta {
namespace async {

//! Compress SI values and write them to the database.
//! This is called on a background thread.
void AsyncNonTimeseriesReport::StatInstValuesWriter::completeTask()
{
    if (using_compression_) {
        z_stream defstream;
        defstream.zalloc = Z_NULL;
        defstream.zfree = Z_NULL;
        defstream.opaque = Z_NULL;

        //Setup the source stream
        auto num_bytes_before = (uint32_t)(si_values_.size() * sizeof(double));
        defstream.avail_in = (uInt)(num_bytes_before);
        defstream.next_in = (Bytef*)(&si_values_[0]);

        //Setup the destination stream
        std::vector<char> compressed_si_values(num_bytes_before);
        defstream.avail_out = (uInt)(compressed_si_values.size());
        defstream.next_out = (Bytef*)(&compressed_si_values[0]);

        //Compress it!
        deflateInit(&defstream, Z_DEFAULT_COMPRESSION);
        deflate(&defstream, Z_FINISH);
        deflateEnd(&defstream);

        //Now send this compressed SI blob to the database.
        compressed_si_values.resize(defstream.total_out);
        const uint32_t original_num_si_values = si_values_.size();

        si_values_writer_->writeCompressedStatisticInstValues(
            compressed_si_values,
            original_num_si_values);
    }

    else {
        si_values_writer_->writeStatisticInstValues(si_values_);
    }
}

} // namespace async
} // namespace sparta
