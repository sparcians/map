// <SIValuesBuffer> -*- C++ -*-

#pragma once

#include "sparta/statistics/StatisticInstance.hpp"
#include "simdb/schema/Schema.hpp"
#include "sparta/report/db/Schema.hpp"

namespace sparta {
namespace statistics {

//! Utility to set all elements of a vector to NaN
inline void refillWithNaNs(std::vector<double> & vec)
{
    vec = std::vector<double>(vec.size(), NAN);
}

/*!
 * \brief This class helps organize contiguous blocks of
 * SI values. These values are buffered at each report
 * update, and they are organized in the buffer so that
 * individual SI's have their values right next to each
 * other. To illustrate, say we had the following CSV
 * file:
 *
 *    si_foo       si_bar       si_biz      si_baz
 *       1.2          450         1000          12
 *       1.4          453         1001          12
 *       1.4          460         1005          14
 *
 * An SIValuesBuffer could be used so that these 12 values
 * appear in this order in a single vector:
 *
 * [1.2, 1.4, 1.4, 450, 453, 460, 1000, 1001, 1005, 12, 12, 14]
 *
 * This is useful when SI values are compressed since data
 * streams with lower entropy tend to compress better than
 * those with higher entropy (depends on compression scheme
 * used). Adjacent SI's will usually display smaller changes
 * from one update to the next, which is why this class buffers
 * them together in column-major format. The equivalent buffer
 * in row-major format would be much more random and would likely
 * show more modest benefits from compression:
 *
 * [1.2, 450, 1000, 12, 1.4, 453, 1001, 12, 1.4, 460, 1005, 14]
 */
class SIValuesBuffer
{
public:
    //! Construct an empty buffer for a given set of SI's, and
    //! the scheduler & root clock the simulation is tied to.
    SIValuesBuffer(const std::vector<const StatisticInstance*> & stats,
                   const Clock & root_clk) :
        stats_(stats),
        scheduler_(*root_clk.getScheduler()),
        root_clk_(root_clk)
    {
        //We are going to be asking the SI's for their values
        //ourselves. Don't take the performance hit of having
        //them writing their values into SnapshotLogger's that
        //nobody is listening to.
        for (const auto si : stats_) {
            si->disableSnapshotLogging();
        }
    }

    //! Switch this buffer to start using row-major ordering
    //! as it fills its internal SI buffers. This is the default
    //! behavior.
    //! \note Must be called when buffersAreEmpty()
    void useRowMajorOrdering() {
        if (!buffersAreEmpty()) {
            throw SpartaException(
                "Cannot change row/column-major ordering when "
                "SIValuesBuffer contains buffered data");
        }
        is_row_major_ = true;
    }

    //! Switch this buffer to start using column-major ordering
    //! as it fills its internal SI buffers.
    //! \note Must be called when buffersAreEmpty()
    void useColumnMajorOrdering() {
        if (!buffersAreEmpty()) {
            throw SpartaException(
                "Cannot change row/column-major ordering when "
                "SIValuesBuffer contains buffered data");
        }
        is_row_major_ = false;
    }

    //! Ask this buffer if it is using row-major or column-
    //! major SI ordering
    db::MajorOrdering getMajorOrdering() const {
        return is_row_major_ ?
            db::MajorOrdering::ROW_MAJOR :
            db::MajorOrdering::COLUMN_MAJOR;
    }

    //! Initialize the number of SI buffers this container
    //! should be able to hold. In the comment above this
    //! class, that string of SI values had *three* buffers
    //! for *four* SI's.
    //!
    //! The number of SI buffers you choose will dictate
    //! how many report updates can hit before this container
    //! is full and needs to be consumed (written to disk).
    void initializeNumSIBuffers(const size_t num_si_buffers) {
        si_values_buffer_.resize(num_si_buffers * stats_.size());
        max_num_si_buffers_ = num_si_buffers;
    }

    //! Tell this SIValuesBuffer to update how many contiguous
    //! blocks of SI's it can hold. This will not take effect
    //! until right after this container is reset/cleared.
    //!
    //!    SIValuesBuffer buf(stats);
    //!    buf.initializeNumSIBuffers(3);
    //!    ...
    //!    buf.bufferCurrentSIValues();    <-- report update
    //!    buf.bufferCurrentSIValues();    <-- report update
    //!    buf.updateNumSIBuffers(2);      <-- no effect yet
    //!    buf.bufferCurrentSIValues();    <-- report update
    //!
    //!    if (buf.buffersAreFilled()) {
    //!        //call getBufferedSIValues() and flush the data
    //!        buf.resetSIBuffers();       <-- resized to 2 SI blocks
    //!    }
    //!
    //!    buf.bufferCurrentSIValues();    <-- report update
    //!    buf.bufferCurrentSIValues();    <-- report update
    //!    buf.bufferCurrentSIValues();    <-- ASSERT! We don't
    //!                                        have space for a
    //!                                        third SI block!
    void updateNumSIBuffers(const size_t num_si_buffers) {
        sparta_assert(num_si_buffers > 0, "You cannot have an "
                    "SIValuesBuffer with zero SI capacity");
        updated_num_si_buffers_ = num_si_buffers;
    }

    //! Ask if this buffer has any room for another SI block.
    //! You should call this before bufferCurrentSIValues()
    //! is called during each report update. If you try to
    //! call bufferCurrentSIValues() and the buffer is full,
    //! it will assert.
    bool buffersAreFilled() const {
        return (current_buffer_write_idx_ == max_num_si_buffers_);
    }

    //! Ask if this buffer is completely empty
    bool buffersAreEmpty() const {
        return (current_buffer_write_idx_ == 0);
    }

    //! Ask this container how many blocks of SI values it
    //! currently has buffered.
    size_t getNumBufferedSIBlocks() const {
        return current_buffer_write_idx_;
    }

    //! Typically, you will only call this method right after
    //! you get all the buffered SI data out of this container
    //! and consume it first.
    //!
    //! This applies any pending updated # of SI blocks that
    //! you set if you previously called updateNumSIBuffers()
    void resetSIBuffers(const bool fill_with_nans = true) {
        current_buffer_write_idx_ = 0;
        if (updated_num_si_buffers_.isValid()) {
            initializeNumSIBuffers(updated_num_si_buffers_);
            updated_num_si_buffers_.clearValid();
        }

        if (fill_with_nans) {
            refillWithNaNs(si_values_buffer_);
        }

        si_buffer_beginning_picoseconds_.clearValid();
        si_buffer_ending_picoseconds_.clearValid();
        si_buffer_beginning_clock_cycles_.clearValid();
        si_buffer_ending_clock_cycles_.clearValid();
    }

    //! Loop over this container's SI's and put their current
    //! values into the buffer. Each SI value will go just to
    //! the right of its previous value. For example:
    //!
    //!    Say the container has 4 SI's, can hold a maximum of
    //!    three report update's worth of SI data, and currently
    //!    2 of those 3 report updates have already hit.
    //!
    //!  [1.2, 1.4, ---, 450, 453, ---, 1000, 1001, ---, 12, 12, ---]
    //!   ***  ***       ***  ***       ****  ****       **  **
    //!     |    |         |    |          |     |        |   |
    //!     -----------------------------------------------   |
    //!          |              |                |        |   |
    //!          |              |                |  Update #1 |
    //!          |              |                |            |
    //!          ----------------------------------------------
    //!                                                       |
    //!                                                 Update #2
    //!
    //!    Then we call 'bufferCurrentSIValues()', and our SI's
    //!    have values 1.4, 460, 1005, and 14 at this moment.
    //!
    //!    We would then have the following SI values at the
    //!    end of the third report update:
    //!
    //!  [1.2, 1.4, 1.4, 450, 453, 460, 1000, 1001, 1005, 12, 12, 14]
    //!             ***            ***              ****          **
    //!               |              |                 |           |
    //!               ----------------------------------------------
    //!                                                            |
    //!                                                      Update #3
    void bufferCurrentSIValues() {
        //Ensure that we have the space in our buffer to append the
        //current SI values
        sparta_assert(current_buffer_write_idx_ < max_num_si_buffers_);

        //Capture the current simulated picoseconds & root clock cycle
        //if this is the first write into a fresh buffer
        if (buffersAreEmpty()) {
            si_buffer_beginning_picoseconds_ = scheduler_.getSimulatedPicoSeconds();
            si_buffer_beginning_clock_cycles_ = root_clk_.currentCycle();
        }

        //For row-major ordering, we start the write index at the Nth SI
        //position, where N equals the current buffer index. For column-
        //major ordering, we start the write index at the 0th position
        //of the Mth buffer, where M equals the current buffer index.
        size_t buffer_idx =
            is_row_major_ ?
            current_buffer_write_idx_ * stats_.size() :
            current_buffer_write_idx_;
        for (const auto si : stats_) {
            si_values_buffer_[buffer_idx] = si->getValue();
            //Row-major ordering jumps the write index ahead by 1.
            //Column-major ordering jumps this index to the next
            //available buffer.
            buffer_idx += is_row_major_ ? 1 : max_num_si_buffers_;
        }
        ++current_buffer_write_idx_;

        //Capture the ending simulated picoseconds & root clock cycle
        //in this buffer
        si_buffer_ending_picoseconds_ = scheduler_.getSimulatedPicoSeconds();
        si_buffer_ending_clock_cycles_ = root_clk_.currentCycle();
    }

    //! Ask this container for all of its buffered SI values. If this
    //! container is empty, it will return a vector of NaN's. If it
    //! is *partially* filled, it will squeeze the SI values like so:
    //!
    //!     Say we have 4 SI's, a maximum of 3 report updates before
    //!     this container is filled, and 2 of those updates have hit.
    //!
    //!   [1.2, 1.4, ---, 450, 453, ---, 1000, 1001, ---, 12, 12, ---]
    //!
    //! If you called this method at this time, it would return a
    //! vector of size 8 (2 report updates * 4 SI's)
    //!
    //!   [1.2, 1.4, 450, 453, 1000, 1001, 12, 12]
    //!
    //! This is more expensive than asking for the buffered data when
    //! the container is full, and we only do this at the end of the
    //! simulation when we need to get any leftover report updates'
    //! SI values out of the buffer and written to disk.
    const std::vector<double> & getBufferedSIValues() {
        if (buffersAreFilled()) {
            return si_values_buffer_;
        }

        if (buffersAreEmpty()) {
            refillWithNaNs(si_values_buffer_);
            return si_values_buffer_;
        }

        const size_t num_filled_buffers = current_buffer_write_idx_;
        squeezed_si_values_.resize(num_filled_buffers * stats_.size());
        auto read_iter = si_values_buffer_.begin();

        //When using row-major ordering, simply copy the SI values
        //buffer as-is into the squeezed SI values vector.
        if (is_row_major_) {
            memcpy(&squeezed_si_values_[0],
                   &si_values_buffer_[0],
                   squeezed_si_values_.size() * sizeof(double));
        } else {
            //Column-major ordering is a little bit different.
            //For each "block" of buffers, whether filled or unfilled...
            for (size_t si_idx = 0; si_idx < stats_.size(); ++si_idx) {
                //...copy over the SI values from the filled buffer slots...
                std::copy(read_iter,
                          read_iter + num_filled_buffers,
                          squeezed_si_values_.begin() + (si_idx * num_filled_buffers));

                //...and jump over the unfilled buffer slots to the start
                //of the next filled buffer.
                std::advance(read_iter, max_num_si_buffers_);
            }
        }

        return squeezed_si_values_;
    }

    //! Get the starting and ending simulated picoseconds and root clock cycle
    //! for the SI's in this buffer.
    //!
    //! This will *assert* if the buffer is empty. Call either buffersAreFilled(),
    //! buffersAreEmpty(), or getNumBufferedSIBlocks() first to see if calling
    //! this method here is even valid.
    void getBeginningAndEndingTimestampsForBufferedSIs(
        uint64_t & starting_picoseconds,
        uint64_t & ending_picoseconds,
        uint64_t & starting_cycles,
        uint64_t & ending_cycles) const
    {
        starting_picoseconds = si_buffer_beginning_picoseconds_;
        ending_picoseconds = si_buffer_ending_picoseconds_;
        starting_cycles = si_buffer_beginning_clock_cycles_;
        ending_cycles = si_buffer_ending_clock_cycles_;
    }

private:
    const std::vector<const StatisticInstance*> stats_;
    std::vector<double> si_values_buffer_;
    std::vector<double> squeezed_si_values_;
    size_t current_buffer_write_idx_ = 0;
    size_t max_num_si_buffers_ = 0;
    utils::ValidValue<size_t> updated_num_si_buffers_;
    bool is_row_major_ = true;

    utils::ValidValue<uint64_t> si_buffer_beginning_picoseconds_;
    utils::ValidValue<uint64_t> si_buffer_ending_picoseconds_;
    utils::ValidValue<uint64_t> si_buffer_beginning_clock_cycles_;
    utils::ValidValue<uint64_t> si_buffer_ending_clock_cycles_;

    //! Simulation's scheduler and root clock. Used in order to get
    //! the current "time values" when we are asked to write the SI
    //! blobs into the database.
    const Scheduler & scheduler_;
    const Clock & root_clk_;

};

} // namespace statistics
} // namespace sparta

