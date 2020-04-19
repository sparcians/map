// <PreloadPkt.h> -*- C++ -*-


/**
 * \file PreloadablePkt.h
 *
 * \brief Define a PreloadPkt dictionary
 *        with value read checking similar to parameters.
 */

#pragma once
#include <unordered_map>
#include "sparta/utils/LexicalCast.hpp"
namespace sparta {
namespace cache {

    /**
     * \class PreloadPkt
     * \brief A hierarchical store interface for preload data.
     *
     *  Also Provides methods for asserting that values are actually
     *  read or ignored.
     *
     *  \todo actually support the value used assertions/warnings
     */
    class PreloadPkt
    {
    public:

        typedef std::shared_ptr<PreloadPkt> NodeHandle; //~ The return type for getMap.
        typedef std::vector<NodeHandle> NodeList; //! The return type for getAsList.
        PreloadPkt() {}
        virtual ~PreloadPkt() {}
        /**
         * \brief Extract a value for a given key using a lexical cast
         *        to the type T.
         */
        template<typename T>
        T getScalar(const std::string& key)
        {
            T val = lexicalCast<T>(getScalarValue_(key), 0);
            return val;
        }

        /**
         * \brief Extract the value for a key that holds a nested
         *        dictionary of more values.
         */
        NodeHandle getMap(const std::string& key)
        {
            return getNestedPkt_(key);
        }

        /**
         * \brief Extract the value for a key as a list of nodes.
         * For example to get the lines you would query,
         * top.cache1.getAsList("lines"), this would return a vector
         * of Map nodes with the scalar nodes for va and data.
         *
         *    top.cache1:
         *       lines:
         *         - va: 0x1000
         *           data: "abc"
         *         - va: 0x2000
         *           data: "bde"
         * \param list A NodeList you would like populated.
         * \param key the string of the key. "lines" in the above example.
         * \return the number of nodes added.
         */
        uint32_t getList(const std::string& key, NodeList& list)
        {
            return getList_(key, list);
        }

        /**
         * \brief Get a list of nodes at the current level
         *        packet's top level.
         */
        uint32_t getList(NodeList& list)
        {
            return getList_(list);
        }

        /**
         * \brief return true if the preload packet actually
         *        has the key.
         */
        virtual bool hasKey(const std::string& key) const = 0;

        /**
         * \brief implement a print method.
         */
        virtual void print(std::ostream&) const = 0;
    protected:
        /**
         * \brief override this method to return scalar values.
         *
         * Scalar values values of any type other than a PreloadPkt.
         * This would be called by the get method above.
         * \param key the string key for the pair.
         */
        virtual std::string getScalarValue_(const std::string& key) const = 0;

        /**
         * \brief override this method to return the value when
         *        the value is a nested PreloadPkt.
         * \param key the string key for the pair.
         */
        virtual NodeHandle getNestedPkt_(const std::string& key) const = 0;

        /**
         * \brief override this method to implement returning a list
         *        of nodes if the key is representing a list.
         */
        virtual uint32_t getList_(const std::string& key, NodeList& list) const = 0;

        /**
         * \brief override this method to return the packet's current
         *        level as a list of packets.
         */
        virtual uint32_t getList_(NodeList& list) const = 0;


    };

    inline std::ostream& operator<< (std::ostream& out, sparta::cache::PreloadPkt const & pkt)
    {
        pkt.print(out);
        return out;
    }

} // namespace cache

} // namespace sparta

