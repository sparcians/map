// <AsyncTimeseriesReport> -*- C++ -*-

#include "sparta/async/AsyncTimeseriesReport.hpp"

//SQLite-specific headers
#include <zlib.h>

//! Compress buffered SI values and write them to the database.
//! This is called on a background thread.
void sparta::async::AsyncTimeseriesReport::CompressedStatInstValuesWriter::completeTask()
{
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

    //How did we do?
    auto num_bytes_after = (uint32_t)(defstream.total_out);

    //Before writing the data to the database, let's tell
    //the async timeseries object how much compression we
    //just saw. This gives it a chance to make some tweaks
    //to its buffers to try for more compression going
    //forward if possible.
    if (post_compression_callback_.isValid()) {
        post_compression_callback_.getValue()(num_bytes_before, num_bytes_after);
    }

    compressed_si_values.resize(num_bytes_after);

    //Now send this compressed SI blob to the database.
    const uint32_t original_num_si_values = si_values_.size();

    db_timeseries_->writeCompressedStatisticInstValuesInTimeRange(
        starting_picoseconds_,
        ending_picoseconds_,
        starting_cycles_,
        ending_cycles_,
        compressed_si_values,
        major_ordering_,
        original_num_si_values);
}
