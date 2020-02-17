// <Collector.hpp> -*- C++ -*-

/**
 * \file Collector.hpp
 *
 * \brief Define a base Collector class.
 */

#ifndef __COLLECTOR_H__
#define __COLLECTOR_H__

#include <string>

namespace sparta{
namespace collection
{
    /**
     * \class Collector
     *
     * \brief A non-templated base class that all Collectors should
     *        inherit from.
     */
    class Collector
    {
    public:
        /**
         * \brief Construct a collector.
         * \param parent the parent tree node for this collector.
         * \param name the name of the collector.
         * \param group the TreeNode group name.
         * \param group_idx the TreeNode group index.
         */
        Collector(const std::string& name) :
            name_(name)
        { }

        //! Enable polymorphism and also be nice
        virtual ~Collector() {}

        std::string getName() const {
            return name_;
        }

    protected:
        /*!
         * The name of the collector. This will be used by many
         * collectors to create child collectable objects
         */
        std::string name_;
    };
}//namespace collection
}//namespace sparta

#endif
