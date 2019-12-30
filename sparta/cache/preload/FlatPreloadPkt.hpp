// <FlatPreloadPkt.h> -*- C++ -*-


/**
 * \file GenericPreloadablePkt.h
 *
 * \brief Implement a PreloadPkt
 *        that just wraps a unordered_map.
 */

#ifndef __MAP_PRELOAD_PKT_H__
#define __MAP_PRELOAD_PKT_H__

#include <unordered_map>

namespace sparta {
namespace cache {

    /**
     * \class FlatPreloadPkt
     * \brief Implement a PreloadPkt that just wraps an unordered_map of
     *        strings.
     * This makes for a flat PreloadPkt, but this makes parsing flat preload
     * files easy. FlatPreloadPkt only supports a single dictionary of scalars.
     */
    class FlatPreloadPkt : public PreloadPkt
    {
    private:
        typedef std::unordered_map<std::string, std::string> Dict;
    public:
        FlatPreloadPkt() :
            PreloadPkt()
        {}

        void addValue(const std::string& key, const std::string& val)
        {
            map_.emplace(key, val);
        }
        void print(std::ostream& ss) const override
        {
            for (auto it = map_.begin(); it != map_.end(); ++it)
            {
                ss << it->first << ": " << it->second << " ";
            }
        }

        bool hasKey(const std::string& key) const override
        {
            auto search = map_.find(key);
            return (search != map_.end());
        }
    private:
        std::string getScalarValue_(const std::string& key) const override
        {
            try
            {
                return map_.at(key);
            }
            catch (std::out_of_range&)
            {
                sparta::SpartaException ex("PreloadPkt does not have key ");
                ex << key;
                throw ex;
            }
        }

        PreloadPkt::NodeHandle getNestedPkt_(const std::string&) const override
        {
            throw sparta::SpartaException("FlatPreloadPkt does not implement nested packets");
        }

        uint32_t getList_(const std::string&, NodeList&) const override
        {
            throw sparta::SpartaException("FlatPreloadPkt does not implement lists");
        }

        uint32_t getList_(NodeList&) const override
        {
            throw sparta::SpartaException("FlatPreloadPkt does not implement list.");
        }
        Dict map_; //! The map of key-value strings.
    };

} // namespace cache
} // namespace sparta

#endif // __MAP_PRELOAD_PKT_H__
