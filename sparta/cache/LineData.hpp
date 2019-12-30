

#ifndef _SPA_CACHE_LINE_DATA_H__
#define _SPA_CACHE_LINE_DATA_H__


#include <cstring>
#include "sparta/utils/ByteOrder.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "cache/BasicCacheItem.hpp"

namespace sparta
{

    namespace cache
    {

        class LineData : public BasicCacheItem
        {
        public:
            // Constructor
            LineData(uint64_t line_size) :
                BasicCacheItem(),
                line_sz_(line_size),
                valid_(false),
                modified_(false),
                exclusive_(false),
                shared_(false),
                data_ptr_(new uint8_t[line_size]),
                data_(data_ptr_.get())
            { }

            // Copy constructor (for deep copy)
            LineData(const LineData &rhs) :
                BasicCacheItem(rhs),
                line_sz_(rhs.line_sz_),
                valid_(rhs.valid_),
                modified_(rhs.modified_),
                exclusive_(rhs.exclusive_),
                shared_(rhs.shared_),
                data_ptr_(new uint8_t[line_sz_]),
                data_(data_ptr_.get())
            {
                memcpy(data_,rhs.data_,line_sz_);
            }

            void reset(uint64_t addr) {
                setValid    ( true );
                setAddr     ( addr );
                setModified ( false );
                setExclusive( true );
                setShared   ( false );
            }

            // Assigment operator (for deep copy)
            LineData &operator=(const LineData &rhs)
            {
                if (&rhs != this) {
                    BasicCacheItem::operator=(rhs);
                    line_sz_ = rhs.line_sz_;
                    valid_ = rhs.valid_;
                    modified_ = rhs.modified_;
                    exclusive_ = rhs.exclusive_;
                    shared_ = rhs.shared_;
                    data_ptr_.reset(new uint8_t[line_sz_]);
                    data_ = data_ptr_.get();
                    memcpy(data_,rhs.data_,line_sz_);
                }

                return *this;
            }

            // Coherency states
            // - Coherency states (MESI) are not known or managed by cache library
            // - isValid() is required by the library
            void setValid(bool v)     { valid_ = v; }
            void setModified(bool m)  { modified_ = m; }
            void setExclusive(bool e) { exclusive_ = e; }
            void setShared(bool s)    { shared_ = s; }
            bool isValid() const     { return valid_; }
            bool isModified() const  { return modified_; }
            bool isExclusive() const { return exclusive_; }
            bool isShared() const    { return shared_; }

            uint64_t getLineSize() const { return line_sz_; }

            uint8_t *getDataPtr() { return data_; }
            const uint8_t *getDataPtr() const { return data_; }

            // The following templatized read & write methods were copied from
            // ArchData.h.  The intention is that there's a common interface
            // to LineData as to all memory object in the simulator.  At this point
            // implementation details are TBD
            template <typename T, ByteOrder BO>
            T read(uint32_t offset, uint32_t idx=0) const {
                uint32_t loc = offset + (idx*sizeof(T));
                sparta_assert(loc + sizeof(T) <= line_sz_);


                const uint8_t* d = data_ + loc;

                T val = *reinterpret_cast<const T*>(d);
                return reorder<T,BO>(val);
            }

            // Copied from ArchData.h.  See comments on read above.
            template <typename T, ByteOrder BO>
            void write(uint32_t offset, const T& t, uint32_t idx=0) {
                uint32_t loc = offset + (idx*sizeof(T));
                sparta_assert(loc + sizeof(T) <= line_sz_);

                uint8_t* d = data_ + loc;

                T& val = *reinterpret_cast<T*>(d);
                val = reorder<T,BO>(t);
            }

            bool read(uint64_t offset,
                      uint32_t size,
                      uint8_t *buf) const
            {
                sparta_assert( (offset + size) <= line_sz_ );
                memcpy(buf, &data_[offset], size);
                return true;
            }

            bool write(uint64_t offset,
                       uint32_t size,
                       const uint8_t *buf )
            {
                sparta_assert( (offset + size) <= line_sz_ );
                memcpy(&data_[offset], buf, size);
                return true;
            }


        private:
            uint64_t line_sz_;
            bool     valid_;
            bool     modified_;
            bool     exclusive_;
            bool     shared_;
            std::unique_ptr<uint8_t[]>  data_ptr_;
            uint8_t * data_ = nullptr;
        }; // class LineData

    }; // namespace cache

}; // namespace sparta

#endif // _SPA_CACHE_LINE_DATA_H__
