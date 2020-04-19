#pragma once

#include "sparta/functional/Register.hpp"
#include "sparta/functional/RegisterSet.hpp"

namespace sparta {

/**
 * \class __RegisterDefintionSet
 *
 * Type that holds a set of register definitions.
 *
 * The invariant of this type is that the last element of definitions_ must be
 * DefinitionT::DEFINITION_END.
 */
template <typename RegisterT>
class __RegisterDefintionSet
{
public:
    using Definition = typename RegisterT::Definition;

    __RegisterDefintionSet()
    {
        definitions_.push_back(RegisterT::DEFINITION_END);
    }

    /**
     * Adds definitions to this definition set.
     *
     * \param definitions Array of definitions to add. Last element must be
     *                    DefinitionT::DEFINITION_END.
     */
    void addDefinitions(const Definition *definitions)
    {
        definitions_.pop_back();

        for (auto i = definitions; *i != RegisterT::DEFINITION_END; ++i) {
            definitions_.push_back(*i);
        }

        definitions_.push_back(RegisterT::DEFINITION_END);
    }

    /**
     * Returns array of the definitions added to this set. The last element is
     * DefinitionT::DEFINITION_END.
     */
    const Definition *getDefinitions() const
    {
        return definitions_.data();
    }

private:
    /** Vector of all definitions addes so far */
    std::vector<Definition> definitions_;
};

using RegisterDefinitionSet =
    __RegisterDefintionSet<sparta::RegisterBase>;

using ProxyDefinitionSet =
    __RegisterDefintionSet<sparta::RegisterProxyBase>;

} /* namespace sparta */
