// <VectorStorage> -*- C++ -*-

#pragma once

#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta::serialization::checkpoint::storage
{

/*!
 * \brief Vector of buffers storage implementation
 */
class VectorStorage
{
    class Segment{
        ArchData::line_idx_type idx_;
        std::vector<char> data_;
        uint32_t bytes_;
    public:

        /*!
         * \brief Copy constructor
         */
        Segment(const Segment&) = default;

        /*!
         * \brief Move constructor
         */
        Segment(Segment&& rhp) :
            idx_(rhp.idx_),
            data_(std::move(rhp.data_)),
            bytes_(rhp.bytes_)
        {
            rhp.idx_ = ArchData::INVALID_LINE_IDX;
            rhp.bytes_ = 0;
        }

        /*!
         * \brief Dummy constructor. Represents null entry (end of ArchData)
         */
        Segment() :
            idx_(ArchData::INVALID_LINE_IDX),
            bytes_(0)
        {;}

        /*!
         * \brief Deleted assignment operator
         */
        Segment& operator=(const Segment& rhp) = delete;

        /*!
         * \brief Data constructor. Allocates data and copies results over
         */
        Segment(ArchData::line_idx_type idx, const char* data, size_t bytes) :
            idx_(idx), bytes_(bytes)
        {
            sparta_assert(idx != ArchData::INVALID_LINE_IDX,
                            "Attempted to create segment of " << bytes << " bytes with invalid line index");
            data_.resize(bytes);
            ::memcpy(data_.data(), data, bytes);
        }

        template <typename Archive>
        void serialize(Archive& ar, const unsigned int /*version*/) {
            ar & idx_;
            ar & data_;
            ar & bytes_;
        }

        ArchData::line_idx_type getLineIdx() const {
            return idx_;
        }

        uint32_t getSize() const {
            return sizeof(decltype(*this)) + bytes_;
        }

        void copyTo(char* buf, uint32_t size) const {
            sparta_assert(size == bytes_, \
                            "Attempted to restore checkpoint data for a line where the "
                            "data was " << bytes_ << " bytes but the loader requested "
                            << size << " bytes. The sizes must match up or something is "
                            "wrong");
            memcpy(buf, data_.data(), bytes_);
        }

        void dump(std::ostream& o) const {
            if(idx_ == ArchData::INVALID_LINE_IDX){
                std::cout << "\nEnd of ArchData";
                return;
            }

            std::cout << "\nLine: " << std::dec << idx_ << " (" << bytes_ << ") bytes";
            for(uint32_t off = 0; off < bytes_;){
                char chr = data_[off];
                if(off % 32 == 0){
                    o << std::endl << std::setw(7) << std::hex << off;
                }
                if(chr == 0){
                    o << ' ' << "..";
                }else{
                    o << ' ' << std::setfill('0') << std::setw(2) << std::hex << (0xff & (uint16_t)chr);
                }
                off++;
            }
        }
    };

    /*!
     * \brief Data segments to restore
     */
    std::vector<Segment> data_;

    /*!
     * \brief Next line index to store when writing lines
     */
    ArchData::line_idx_type next_idx_ = ArchData::INVALID_LINE_IDX;

    /*!
     * \brief Index in data_ of next line to restore in nextRestoreLine
     */
    uint32_t next_restore_idx_ = 0;

    /*!
     * \brief iterator in data_ of line being read by call to readLineData.
     * Is always next_restore_idx_ or one less.
     */
    decltype(data_)::const_iterator cur_restore_itr_;

public:
    VectorStorage() {
    }

    ~VectorStorage() {
    }

    VectorStorage(const VectorStorage&) = default;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int /*version*/) {
        ar & data_;
    }

    void dump(std::ostream& o) const {
        for(auto const &seg : data_){
            seg.dump(o);
        }
    }

    uint32_t getSize() const {
        uint32_t bytes = sizeof(decltype(*this));
        for(Segment const & seg : data_){
            bytes += seg.getSize();
        }
        return bytes;
    }

    void prepareForLoad() {
        next_restore_idx_ = 0;
        cur_restore_itr_ = data_.begin();
    }

    void beginLine(ArchData::line_idx_type idx) {
        sparta_assert(idx != ArchData::INVALID_LINE_IDX,
                        "Cannot begin line with INVALID_LINE_IDX index");
        next_idx_ = idx;
    }

    void writeLineBytes(const char* data, size_t size) {
        sparta_assert(data_.size() == 0 || data_.back().getLineIdx() != next_idx_,
                        "Cannot store the same line idx twice in a checkpoint. Line "
                        << next_idx_ << " detected twice in a row");
        sparta_assert(next_idx_ != ArchData::INVALID_LINE_IDX,
                        "Cannot write line bytes with INVALID_LINE_IDX index");
        data_.emplace_back(next_idx_, data, size);
    }

    /*!
     * \brief Signals end of this checkpoint's data for one ArchData
     */
    void endArchData() {
        data_.emplace_back();
    }

    /*!
     * \brief Is the reading state of this storage good? (i.e. haven't tried
     * to read past the end of the data)
     */
    bool good() const {
        return next_restore_idx_ <= data_.size(); // Not past end of stream
    }

    /*!
     * \brief Restore next line. Return ArchData::INVALID_LINE_IDX on
     * end of data.
     */
    ArchData::line_idx_type getNextRestoreLine() {
        if(next_restore_idx_ == data_.size()){
            next_restore_idx_++; // Increment to detect errors
            return ArchData::INVALID_LINE_IDX; // Done with restore
        }else if(next_restore_idx_ > data_.size()){ // Past the end
            throw SpartaException("Failed to restore a checkpoint because ")
                << "caller tried to keep getting next line even after "
                "reaching the end of the restore data";
        }
        if(next_restore_idx_ != 0){
            cur_restore_itr_++;
        }
        next_restore_idx_++;

        const auto next_line_idx = cur_restore_itr_->getLineIdx(); // May be invalid to indicate end of ArchData
        return next_line_idx;
    };

    /*!
     * \brief Read bytes for the current line
     */
    void copyLineBytes(char* buf, uint32_t size) {
        sparta_assert(cur_restore_itr_ != data_.end(),
                        "Attempted to copy line bytes from an invalid line iterator");
        sparta_assert(cur_restore_itr_->getLineIdx() != ArchData::INVALID_LINE_IDX,
                        "About to return line from checkpoint data segment with INVALID_LINE_IDX index");
        cur_restore_itr_->copyTo(buf, size);
    }

};

} // namespace sparta::serialization::checkpoint::storage
