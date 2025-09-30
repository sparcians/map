// <StringStreamStorage> -*- C++ -*-

#pragma once

#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta::serialization::checkpoint::storage
{

/*!
 * \brief Stringstream storage implementation
 * \warning This is deprecated in favor of VectorStorage for in-memory uses.
 * However, this is a starting point for disk-based storage schemes
 */
class StringStreamStorage
{
    std::stringstream ss_;

public:
    StringStreamStorage() {
        ss_.exceptions(std::ostream::eofbit | std::ostream::badbit |
                       std::ostream::failbit | std::ostream::goodbit);
    }

    void dump(std::ostream& o) const {
        auto s = ss_.str();
        auto itr = s.begin();
        for(; itr != s.end(); itr++){
            char chr = *itr;
            if(chr == 'L'){
                uint32_t off = 0;
                ArchData::line_idx_type ln_idx;
                strncpy((char*)&ln_idx, s.substr(itr-s.begin(), sizeof(ln_idx)).c_str(), sizeof(ln_idx));
                std::cout << "\nLine: " << ln_idx << std::endl;
                itr += sizeof(ArchData::line_idx_type);

                for(uint16_t i=0; i<64; ++i){
                    chr = *itr;
                    if(off % 32 == 0){
                        o << std::setw(7) << std::hex << off;
                    }
                    if(chr == 0){
                        o << ' ' << "..";
                    }else{
                        o << ' ' << std::setfill('0') << std::setw(2) << std::hex << (0xff & (uint16_t)chr);
                    }
                    off++;
                    if(off % 32 == 0){
                        o << std::endl;
                    }
                    ++itr;
                }
            }
        }
    }

    uint32_t getSize() const {
        return ss_.str().size() + sizeof(decltype(*this));
    }

    void prepareForLoad() {
        ss_.seekg(0); // Seek to start with get pointer before consuming
    }

    void beginLine(ArchData::line_idx_type idx) {
        ss_ << 'L'; // Line start char

        ArchData::line_idx_type idx_repr = reorder<ArchData::line_idx_type, LE>(idx);
        ss_.write((char*)&idx_repr, sizeof(ArchData::line_idx_type));
    }

    void writeLineBytes(const char* data, size_t size) {
        ss_.write(data, size);
    }

    /*!
     * \brief Signals end of this checkpoint's data
     */
    void endArchData() {
        ss_ << "E"; // Indicates end of this checkpoint data

        sparta_assert(ss_.good(),
                        "Ostream error while writing checkpoint data");
    }

    /*!
     * \brief Is the reading state of this storage good? (i.e. haven't tried
     * to read past the end of the data)
     */
    bool good() const {
        return ss_.good();
    }

    /*!
     * \brief Restore next line. Return ArchData::INVALID_LINE_IDX on
     * end of data.
     */
    ArchData::line_idx_type getNextRestoreLine() {
        char ctrl;
        ss_ >> ctrl;
        sparta_assert(ss_.good(),
                        "Encountered checkpoint data stream error or eof");
        if(ctrl == 'L'){
            ArchData::line_idx_type ln_idx = 0;
            ss_.read((char*)&ln_idx, sizeof(ln_idx)); // Presumed LE encoding
            return ln_idx;
        }else if(ctrl == 'E'){
            return ArchData::INVALID_LINE_IDX; // Done with restore
        }else{
            throw SpartaException("Failed to restore a checkpoint because a '")
                << ctrl << "' control character was found where an 'L' or 'E' was found";
        }
    };

    /*!
     * \brief Read bytes for the current line
     */
    void copyLineBytes(char* buf, uint32_t size) {
        ss_.read(buf, size);
    }
};

} // namespace sparta::serialization::checkpoint::storage
