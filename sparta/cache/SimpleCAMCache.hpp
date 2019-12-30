#ifndef _SIMPLE_CAM_CACHE_H__
#define _SIMPLE_CAM_CACHE_H__


#include <string>
#include <vector>
#include "cache/ReplacementIF.hpp"
#include "cache/TrueLRUReplacement.hpp"

namespace sparta
{

    namespace cache
    {



        template< class CacheItemT,
                  class TagT  = uint64_t >
        class SimpleCAMCache
        {
        public:

            SimpleCAMCache( const CacheItemT    &default_line,
                            const ReplacementIF &rep)
            {
                num_lines_ = rep.getNumWays();
                rep_.reset(rep.clone());
                lines_.resize(num_lines_, default_line);

                for (uint32_t i=0; i<num_lines_; ++i) {
                    lines_[i].setWayNum(i);
                }
            }

            ~SimpleCAMCache() {};

            /**
             *\return Pointer to line with addr.  nullptr is returned if not fould
             */
            CacheItemT * getLine(TagT tag)
            {
                CacheItemT *line = nullptr;
                for (uint32_t i=0;i<num_lines_; ++i) {
                    if ( lines_[i].isValid() ) {
                        if  ( lines_[i].getTag() == tag ) {
                            line = &lines_[i];
                            break;
                        }
                    }
                }
                return line;
            }

            /**
             *\returns true on hit, false on miss. On hit, fills the function argument vector with pointers to lines that match the tag.
             */
            bool getLines(TagT tag, std::vector<CacheItemT>& lines)
            {
                CacheItemT *line = nullptr;
                bool hit = false;
                for (uint32_t i=0;i<num_lines_; ++i) {
                    if ( lines_[i].isValid() ) {
                        if  ( lines_[i].getTag() == tag ) {
                            lines.push_back(&lines_[i]);
                            hit = true;
                        }
                    }
                }

                return hit;
            }

            /**
             *\return Pointer to line with addr.  nullptr is returned if not fould
             */
            const CacheItemT * peekLine(TagT tag) const
            {
                const CacheItemT *line = nullptr;
                for (uint32_t i=0;i<num_lines_; ++i) {

                    if ( lines_[i].isValid() &&
                         (lines_[i].getTag() == tag) ) {
                        line = &lines_[i];
                        break;
                    }
                }
                return line;
            }

            CacheItemT * getLineByWay(const uint32_t& way)
            {
                sparta_assert(way < num_lines_);
                return &lines_[way];
            }

            CacheItemT &getLRULine()
            {
                uint32_t way = rep_->getLRUWay();
                return lines_[way];
            }

            const CacheItemT &peekLRULine() const
            {
                uint32_t way = rep_->getLRUWay();
                return lines_[way];
            }

            CacheItemT &getMRULine()
            {
                uint32_t way = rep_->getMRUWay();
                return lines_[way];
            }

            const CacheItemT &peekMRULine() const
            {
                uint32_t way = rep_->getMRUWay();
                return lines_[way];
            }

            /**
             *\return whether an addr is in the cache
             */
            bool isHit(TagT tag) const
            {
                const CacheItemT *line = peekLine(tag);
                return ( line != nullptr );
            }

            // 'line' must be part of cache (obtained with getLine or peekLine)
            void touchLRU(const CacheItemT &line)
            {
                rep_->touchLRU( line.getWay() );
            }

            // 'line' must be part of cache (obtained with getLine or peekLine)
            void touchMRU(const CacheItemT &line)
            {
                rep_->touchMRU( line.getWay() );
            }



            void invalidateLineWithLRUUpdate(CacheItemT &line)
            {
                line.reset( 0 );
                line.setValid( false );
                touchLRU( line );
            }

            void invalidateAll()
            {
                auto it = lines_.begin();
                for (; it != lines_.end(); ++it) {
                    it->setValid(false);
                }
                rep_->reset();
           }

        protected:
            uint32_t                num_lines_ = 0;
            std::vector<CacheItemT> lines_;
            std::unique_ptr<ReplacementIF> rep_ = nullptr;

         }; // class SimpleCache2



    }; // namespace cache

}; // namespace sparta

#endif // _SIMPLE_CAM_CACHE_H__
