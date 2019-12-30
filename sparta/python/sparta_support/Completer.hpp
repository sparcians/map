// <Completer> -*- C++ -*-


/*!
 * \file Completer.h
 * \brief Basic Completer singleton class for various argument completers.
 */

#ifndef __COMPLETER_H__
#define __COMPLETER_H__

#include <string>
#include "sparta/simulation/ParameterTree.hpp"

namespace sparta
{
    template<typename TargetType>
    /*!
     * \brief Singleton Completer class template which holds
     *  the actual completer-instances for various target-types.
     */
    class Completer{
    public:

        /*!
         * \brief Not copy-constructable
         */
        Completer(const Completer&) = delete;

        /*!
         * \brief Not move-constructable
         */
        Completer(Completer&&) = delete;

        /*!
         * \brief Not copy-assignable
         */
        Completer& operator = (const Completer&) = delete;

        /*!
         * \brief Not move-assignable
         */
        Completer& operator = (Completer&&) = delete;

        /*!
         * \brief Returns a singleton instance of the completer.
         */
        static Completer& getCompleter(){
            static Completer completer;
            return completer;
        }

        /*!
         * \brief Returns an instance of target-type completer.
         */
        TargetType& getTargetCompleter(){
            return instance_;
        }

    private:

        /*!
         * \brief Private default-constructor. The target-type completer
         +  must also be default-constructible.
         */
        Completer() : instance_(){}
        TargetType instance_;
    };

    // Wrappers to create instances of the above class for use in the python shell

    /*!
     * \brief Update the IPython regex engine with the address of the tree
     * and the new concrete path that has been created.
     */
    void updateCompleter(const std::string& path, sparta::ParameterTree* tree);

    /*!
     * \brief Build the IPython regex engine from a .yaml file.
     */
    void buildCompleter(const sparta::ParameterTree& tree,
                        sparta::ParameterTree* add_tree,
                        const std::string& tree_type,
                        const std::string& pattern = "");

}

#endif
