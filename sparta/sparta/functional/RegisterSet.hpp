// <RegisterSet> -*- C++ -*-

#pragma once

#include <cstdint>
#include <iostream>
#include <ios>
#include <unordered_map>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sparta/functional/Register.hpp"
#include "sparta/functional/RegisterBankTable.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/StringManager.hpp"

namespace sparta {

class RegisterSet;

/*!
 * \class RegisterProxyBase
 *
 * RegisterProxy needs to define DEFINITION_END. However, since it is
 * templatized it needs to define DEFINITION_END inline but C++ does not seem to
 * allow inline definition of class variables for template classes. This class,
 * RegisterProxyBase, exists only so that RegisterProxy can derive from it and
 * that way get a definition of DEFINITION_END.
 */
class RegisterProxyBase
{
public:
    /*!
     * \brief Describes a register Proxy.
     *
     * Can be constructed with initializer list as in:
     *
     * \code
     * sparta::RegisterProxy::Definition proxies[] =
     *     { {1001, "reg1", 0, GROUP_REG, "reg", 1, "This is Register 1"},
     *       {1002, "reg2", 0, GROUP_REG, "reg", 2, "This is Register 2"}
     *       sparta::RegisterProxy::DEFINITION_END };
     * \endcode
     */
    struct Definition
    {
        /*!
         * \brief ID. Must be unique within all registers and register proxies
         * within a register set
         */
        const Register::ident_type id;

        /*!
         * \brief String identifier for this register proxy. This name must be
         * unique from all other RegisterProxies in the same RegisterSet as well
         * as all other Registers in this RegisterSet.
         * TreeNode::validateName. Must NOT be NULL.
         */
        const char *name;

        /*!
         * \brief Group number by which the a Register will be looked up when
         * this RegisterProxy is accessed
         * \see Register::Definition::group_num
         */
        const Register::group_num_type group_num;

        /*!
         * \brief String name of group in which this register resides
         * (e.g. gpr). Must NOT be NULL. This is the same as (and is validated
         * like) Register::Definition::group.
         */
        const char *group;

        /*!
         * \brief Group index by which the a Register will be looked up when
         * this RegisterProxy is accessed
         * \see Register::Definition::group_num
         */
        const Register::group_idx_type group_idx;

        /*!
         * \brief Description of this proxy as if it were a Register. Must NOT
         * be NULL.
         */
        const char *desc;

    }; // Definition

    //! Entry indicating the end of a sparta::Register::Definition array
    static const Definition DEFINITION_END;
};

inline bool operator==(const RegisterProxyBase::Definition &a,
                       const RegisterProxyBase::Definition &b)
{
    if (a.id != b.id) {
        return false;
    }
    if (!utils::strcmp_with_null(a.name, b.name)) {
        return false;
    }
    if (a.group_num != b.group_num) {
        return false;
    }
    if (!utils::strcmp_with_null(a.group, b.group)) {
        return false;
    }
    if (a.group_idx != b.group_idx) {
        return false;
    }
    if (!utils::strcmp_with_null(a.desc, b.desc)) {
        return false;
    }

    return true;
}

inline bool operator!=(const RegisterProxyBase::Definition &a,
                       const RegisterProxyBase::Definition &b)
{
    return !(a == b);
}

/*!
 * \brief Represents an interface to a pseudo-"Register" of a fixed size which
 * indirectly references other registers (having the same group num, group idx,
 * and size) depending on simulator state. Essentially, this hides banking and
 * register aliasing (shared data) information from the client of this interface
 * which is useful for presenting debugginer-visible and software-visible
 * registers without regard for the underlying banking or aliasing.
 * \todo Access a register
 */
class RegisterProxy : public RegisterProxyBase
{
public:
    /*!
     * \brief RegisterProxy disabled
     */
    RegisterProxy() = delete;

    /*!
     * \brief Move construction not allowed
     */
    RegisterProxy(RegisterProxy &&) = delete;

    /*!
     * \brief Not Copy Constructable
     */
    RegisterProxy(const RegisterProxy &) = delete;

    /*!
     * \brief Assignment Operator deleted to assure persistence of any proxy's
     * group information and RegisterSet
     */
    RegisterProxy &operator=(const RegisterProxy &) = delete;

    /*!
     * \brief Concrete Register Constructor. The "current register" of this
     * proxy never changes.
     * \param reg Register to refer to in proxy. This will invariably be
     * the result of getCurrentRegister()
     */
    RegisterProxy(RegisterBase &reg)
    : rs_(*reg.getParentAs<RegisterSet>())
    , reg_inv_(&reg)
    , group_num_(Register::GROUP_NUM_NONE)
    , group_idx_(Register::GROUP_IDX_NONE)
    , reg_name_(reg.getNamePtr())
    {
        sparta_assert(
            reg_inv_ != nullptr,
            "reg argument of RegisterProxy constructor must not be nullptr")
    }

    /*!
     * \brief Actual Proxy Contruction. Proxies all registers having a specific
     * group_num and group_idx in a single RegisterSet. The "current register"
     * of this proxy switches between banks based on the result of
     * RegisterSet::getCurrentBank()
     * \param rs RegisterSet in which current register will be looked up with
     * the group number and index specified in this constructor
     * \param group_num Group number. Must not be Register::GROUP_NUM_NONE
     * \param group_num Group number. Must not be Register::GROUP_IDX_NONE
     * \param managed_name Register name interned in the sparta::StringManager
     * singleton
     */
    RegisterProxy(RegisterSet &rs,
                  Register::group_num_type group_num,
                  Register::group_idx_type group_idx,
                  const std::string *managed_name)
    : rs_(rs)
    , reg_inv_(nullptr)
    , group_num_(group_num)
    , group_idx_(group_idx)
    , reg_name_(managed_name)
    {
        sparta_assert(group_num_ != Register::GROUP_NUM_NONE,
                    "group_num argument of RegisterProxy constructor must not "
                    "be GROUP_NUM_NONE");
        sparta_assert(group_idx_ != Register::GROUP_IDX_NONE,
                    "group_idx argument of RegisterProxy constructor must not "
                    "be GROUP_IDX_NONE");
        sparta_assert(reg_name_ != nullptr, "managed_name argument of "
                                          "RegisterProxy constructor must not "
                                          "be nullptr");
        sparta_assert(StringManager::getStringManager().isInterned(managed_name),
                    "managed_name argument of RegisterProxy constructor must "
                    "be a string managed by the StringManager singleton");
    }

    /*!
     * \brief Destructor
     */
    ~RegisterProxy()
    {
    }

    /*!
     * \brief Form a string representing this proxy
     */
    std::string stringize() const;

    /*!
     * \brief Gets the current register being pointed to by this proxy based on
     * simulation state
     * \throw SpartaException if the current register cannot be retrieved because
     * it does not exist in the current context indicated by the simulator
     */
    RegisterBase *getCurrentRegister() const;

    /*!
     * \brief Attempt to get the current register being pointed to by this proxy
     * based on simulation state.
     * \return The current register being proxied if any. Returns nullptr if no
     * register exists in the current context indicated by the simulator
     */
    RegisterBase *tryGetCurrentRegister() const;

    /*!
     * \brief Gets the RegisterSet which this proxy accesses
     */
    RegisterSet &getContainingRegisterSet() const;

private:
    /*!
     * \brief RegisterSet containing the register(s) being proxied
     */
    RegisterSet &rs_;

    /*!
     * \brief Register invariant. Proxy always points to this register if this
     * is not nullptr
     */
    RegisterBase *reg_inv_;

    /*!
     * \brief Group number of register to proxy
     */
    typename RegisterBase::group_num_type group_num_;

    /*!
     * \brief Group idx of register to proxy
     */
    typename RegisterBase::group_idx_type group_idx_;

    /*!
     * \brief Pointer to interned string name of the registered proxied
     */
    const std::string *reg_name_;
}; // class RegisterProxy

/*!
\page RegisterProxy_Usage RegisterProxy Usage
sparta::RegisterProxy Allows interaction with a register "prototype" (name, group, index)
through a proxy object which automatically chooses a sparta::Register from a variable bank in a
RegisterSet based on the state of the simulation (as retrieved through a callback function in
RegisterSet). A single register proxy may indirectly refer to registers of different sizes

Note that proxies only need to be defined for those "registers" which must support switching
dynamically between banks, but must still have a common register name at all times for
register-lookup reasons. Otherwise, differently named registers should be used for each bank.
This is useful for providing a set of software visible registers without exposing the banking
system.

\par 1. Defining Proxies
RegisterProxy objects are explicitly defined by creating a vector of sparta::RegisterProxy::Definition
objects much like sparta::Register::Definition. This table of definitions is then given to a
RegisterSet as a consruction argument along with a callback function of type
RegisterSet::CurrentBankFunction, which provides the RegisterSet instance the means of querying the
owning simulator about the current bank.

Instantiating a RegisterSet with a table of sparta::RegisterProxy::Definitions requires use of an
extended constructor:
\code
RegisterSet myregset(parent_node, reg_defs, proxy_defs, get_bank_fxn);
\endcode

The proxy_defs table is an array of RegisterProxy::Definition objects terminated by a
RegisterProxy::DEFINITION_END.
The following declares a single register proxy, and requires that the \a reg_defs table used in the
same RegisterSet have at least one 4-byte register with group_num=4, group_name="group" and
group_idx=2. If such a register does not exist an exception will be thrown during RegisterSet
constrution. Note that names and IDs of register proxies must be unique from all other proxy and
register names.

\code
// Proxy Group 4, index 2.
// Error if any registers with this same group/ idx combination or have a group name
// other than "group".
RegisterProxy::Definition proxy_defs[] = {
    { 30001, "reg", 4, "group", 2, "example PROXY"},
    RegisterProxy::DEFINITION_END
};
\endcode

The get_bank_fxn argument to the RegisterSet constructor is a RegisterSet::CurrentBankFunction. It
will implicitly accept static funtions having the correct signature. It accepts nullptr to indicate
that Register::BANK_IDX_DEFAULT will always be used.

The arguments have the following meanings:
\li group_num Group number of register requested
\li group_idx Group index of register requested
\li name_ptr Pointer to string interned in sparta::StringManager singleton (unless nullptr) so that
pointer comparisons can be used instead of string comparisons.

It will accept correctly formed lambda functions:

\code
sparta::Register::bank_idx_type cur_bank = 0;
auto get_bank_fxn = [&](Register::group_num_type,
                        Register::group_idx_type,
                        const std::string*) {
    return cur_bank;
};
\endcode

And finally, std::bind can be used to pass a bound member function:

\code
class BankGetter {
public:
    Register::bank_idx_type getBank(Register::group_num_type,
                                    Register::group_idx_type,
                                    const std::string*) {
        return 1;
    }
};

// ...

BankGetter bg_instance;
RegisterSet::CurrentBankFunction get_bank_fxn  = std::bind(&BankGetter::getBank,
                                                           &bg_instance,
                                                           std::placeholders::_1,
                                                           std::placeholders::_2,
                                                           std::placeholders::_3);
\endcode

The functionality of the get_bank_fxn can be tested through RegisterSet with
\code
myregset.getCurrentBank(<some_group>,<some_idx>,<name pointer>);
\endcode

\par 2. Register Name Lookup Using Proxies
Use sparta::RegisterSet::getRegisterProxy to get a sparta::RegisterProxy when one is needed. For most
purposes, this replaces RegisterSet::getRegister. RegisterSet::getRegister will <b>NOT</b> return
any RegisterProxies, so it is insufficient for certain tasks when a RegisterSet has dynamic
registers requiring RegisterProxies. Because Register names are unique, if getRegisterProxy is
called with the name of a regular Register, it will return a RegisterProxy that always points to the
register with the given name. In this way, RegisterSet::getRegisterProxy abstracts away the details
of which registers are proxies and which are regular Registers.

The RegisterProxy references returned by this function cannot be copied. However, the reference
returned refers to instances which are owned by the RegisterSet and can be referred to until that
RegisterSet is destroyed.

\code
sparta::RegisterProxy* prox = &myregset.getRegisterProxy("reg");
// OR...
sparta::RegisterProxy& prox = myregset.getRegisterProxy("reg");
\endcode

The argument to getRegisterProxy can be the name of a normal Register as well, without requiring
any changes in the client code. This allows registers to be turned into proxies and vise-versa
transparently - requiring only a name change in the original register definition table and an entry
in the register proxy table.

\par 3. Accessing the Actual Register
At any time, the RegisterProxy instance can be queried to get the real Register to which it
currently points.

\code
// Throws if no register is being proxied: If the bank index supplied by the get_bank_fxn
// referred to a bank with no register having this proxy's group/index.
Register* r = rset.getRegisterProxy("reg").getCurrentRegister();

// Returns nullptr on missing Register instead of throwing an exception
Register* r = rset.tryGetRegisterProxy("reg").getCurrentRegister();
\endcode

 */



/*!
 * \brief Holds and can create a set of Register objects having various
 * names and groups.
 *
 * Contains an ArchData instance to hold all registers it contains
 */
class RegisterSet : public TreeNode
{
public:

    /*!
     * \brief Size of an ArchData line for Registers (bytes)
     * Must be large enough to fit the largest register in a Register set.
     * Increase this value if larger registers must be supported.
     * This value could also be a construction argument if really needed.
     */
    static const ArchData::offset_type ARCH_DATA_LINE_SIZE = 512;

    //! Type for holding registers
    using RegisterVector = typename RegisterBankTable<RegisterBase>::RegisterVector;

    /*!
     * \brief Vector of RegisterVectors used for lookup by numeric group
     * number of type Register::Definition::group_num
     */
    using GroupVector = typename RegisterBankTable<RegisterBase>::RegisterVector;

    /*!
     * \brief Map of strings to self-deleting RegisterProxy instances
     */
    typedef std::unordered_map<std::string, std::unique_ptr<RegisterProxy>> RegisterProxyMap;

    /*!
     * \brief Function object type for holding a callback for querying the
     * owning simulator about the current bank.
     */
    typedef std::function<
        Register::bank_idx_type(RegisterBase::group_num_type,
                                RegisterBase::group_idx_type,
                                const std::string *name_ptr)>
        CurrentBankFunction;

    /*!
     * Used to enable the constructors do deduce their template parameters
     */
    template <typename RegisterT>
    class RegisterTypeTag
    {
    };

    /*!
     * \brief Constructor
     * \param parent Parent node
     * \param defs Array of register definitions which ends with a
     * Register::DEFINITION_END entry.
     * \param proxy_defs Array of RegisterProxy definitions which ends with a
     * RegisterProxy::DEFINITION_END entry. These proxies will be available
     * through getRegisterProxy
     * \param cbfxn Function for querying the current bank of an owning
     * simulator Based on the current context of the simulation, this function
     * should return a bank index for register lookup - usually by a
     * RegisterProxy.
     * \post ArchData owned by this RegisterSet will be laid out and can no
     * longer be modified. This means that no more registers can be added.
     */
    template <typename RegisterT>
    RegisterSet(TreeNode *parent,
                const RegisterBase::Definition *defs,
                const RegisterProxyBase::Definition *proxy_defs,
                CurrentBankFunction cbfxn,
                RegisterTypeTag<RegisterT> tag)
    : TreeNode("regs",
               TreeNode::GROUP_NAME_BUILTIN,
               TreeNode::GROUP_IDX_NONE,
               "Register set")
    , adata_(this,
             ARCH_DATA_LINE_SIZE,
             ArchData::DEFAULT_INITIAL_FILL,
             ArchData::DEFAULT_INITIAL_FILL_SIZE,
             false) // Cannot delete lines
    , cur_bank_fxn_(cbfxn)
    {
        (void)tag;

        if(parent){
            setExpectedParent_(parent);
        }

        // Add all registers
        const auto *rdef = defs;
        if (rdef != nullptr) {
            while (rdef->name != nullptr) {
                addRegister_<RegisterT>(rdef);
                ++rdef;
            }
        }

        // Add all proxies
        const auto *pdef = proxy_defs;
        if (pdef != nullptr) {
            while (pdef->name != nullptr) {
                addProxy_(pdef);
                ++pdef;
            }
        }

        // Perform the layout. At this point, no further registers can be added.
        adata_.layout();

        if (parent) {
            parent->addChild(this);
        }
    }

    /*!
     * \brief Constructor with no current-bank query function
     */
    template <typename RegisterT>
    RegisterSet(TreeNode *parent,
                const RegisterBase::Definition *defs,
                RegisterTypeTag<RegisterT> tag)
    : RegisterSet(parent, defs, nullptr, nullptr, tag)
    {
        // Handled in delegated consturctor
    }

    template <typename RegisterT = Register>
    static std::unique_ptr<RegisterSet>
    create(TreeNode *parent,
           const RegisterBase::Definition *defs,
           const RegisterProxyBase::Definition *proxy_defs,
           CurrentBankFunction cbfxn)
    {
        return std::unique_ptr<RegisterSet>(new RegisterSet(
            parent, defs, proxy_defs, cbfxn, RegisterTypeTag<RegisterT>()));
    }

    template <typename RegisterT = Register>
    static std::unique_ptr<RegisterSet>
    create(TreeNode *parent, const RegisterBase::Definition *defs)
    {
        return std::unique_ptr<RegisterSet>(new RegisterSet(
            parent, defs, RegisterTypeTag<RegisterT>()));
    }

    /*!
     * \brief Destructor
     *
     * Deletes all registers allocated by this instance
     */
    ~RegisterSet()
    {
        for (auto r : owned_regs_) {
            delete r;
        }
    }

    /*!
     * \brief Reset all registers in this set to default values
     */
    void reset() {
        for (uint32_t i=0; i<regs_.size(); i++) {
            regs_[i]->reset();
        }
    }

    /*!
     * \brief Sets the minimum bank index for this register set, overriding
     * the default of Register::BANK_IDX_DEFAULT.
     * \param min_idx Minimum bank index to be accessible. This is an index,
     * so the minimum number of banks required will be \a min_idx+1
     * \note This can only increase the number of banks currently
     * represented
     *
     * This ensures that a minimum number of banks are instantiated in this
     * RegisterSet. Register set definitions can cause more banks to be
     * instantiated though.
     *
     * This is required if banked operation is required, but for som reason
     * all registers in the RegisterSet are unbanked (available in every
     * bank) because all of their definitions' bank_membership list are
     * empty.
     */
    void setMinimumBankIndex(RegisterBase::bank_idx_type min_idx) {
        banks_.setMinimumBankIndex(min_idx);
    }

    /*!
     * \brief Gets the number of counters in this Set
     */
    uint32_t getNumRegisters() const { return regs_.size(); }

    /*!
     * \brief Returns the number of banks created (empty banks are automatically
     * created between sprarse bank indices in register definitions)
     */
    RegisterBase::bank_idx_type getNumBanks() const
    {
        return banks_.getNumBanks();
    }

    /*!
     * \brief Returns the number of groups in this register set
     */
    RegisterBase::group_idx_type getNumGroups() const
    {
        return banks_.getNumGroups();
    }

    /*!
     * \brief Gets the vector of Registers contained by this set.
     * \note There is no non-const version of this method. Modifying this
     * vector externally should never be allowed
     */
    const RegisterVector& getRegisters() const { return regs_; }

    // Overload of TreeNode::stringize
    virtual std::string stringize(bool pretty=false) const override {
        (void) pretty;
        std::stringstream ss;
        ss << '<' << getLocation() << ' ' << regs_.size() << " regs>";
        return ss.str();
    }

    /*!
     * \brief Dumps the table of banks and registers
     */
    void dumpBanks(std::ostream& out) {
        banks_.dump(out);
    }

    /*!
     * \brief Retrieves a concrete register child that is a Register with the
     * given dotted path.
     * \note Will not find any register proxies. These proxies are created by
     * populating registers definitions a certain way (see Register::Definition)
     * \note no pattern matching supported in this method
     * \throw SpartaException if a register child by this name is not found
     * \see getRegisterProxy
     */
    RegisterBase *getRegister(const std::string &name)
    {
        return getChildAs<RegisterBase>(name);
    }

    /*!
     * \brief Retrieves a register proxy based on the given name.
     * RegisterProxies are an indirection layer that direct acccesses to
     * registers in various banks depending on the state of the simulator that
     * owns them.
     * \param name Any valid Register name or the name of a Registered defined
     * as a proxy (see Register::Definition)
     * \note Using a RegisterProxy add slight overhead to register accesses and
     * should generally be a debugging/ui/verification tool
     * \return RegisterProxy reference owned by this RegisterSet. A
     * RegisterProxy returned will remain valid until this RegisterSet is
     * destructed. It is safe to take the address of this result and store it
     */
    RegisterProxy &getRegisterProxy(const std::string &name)
    {
        auto rpi = reg_proxies_.find(name);
        if(rpi != reg_proxies_.end()){
            return *(rpi->second.get());
        }
        auto r = getChildAs<RegisterBase>(name, false);
        if(!r){
            throw SpartaException("Could not get register proxy from ") << getLocation()
                  << " named \"" << name << "\" because there was no existing proxy and no "
                  "register with this name.";
        }
        reg_proxies_[name] =
            typename RegisterProxyMap::mapped_type(new RegisterProxy(*r));
        return *reg_proxies_[name];
    }

    /*!
     * \brief Determines if a register exists with the given group number
     * and index.
     * \param group_num Group identifier number (e.g. 0 for GPRs)
     * \param group_idx Index within the given group number (e.g. 0 for
     * register named "r0")
     * \note Assumes bank 0, which will awlays exist
     * \return true if the register described exists and can safely be
     * retrieved with lookupRegister.
     * \see Register::getGroupNum
     * \see TreeNode::getGroupIndex
     */
    bool canLookupRegister(RegisterBase::group_num_type group_num,
                           RegisterBase::group_idx_type group_idx) const noexcept
    {
        return banks_.canLookupRegister(group_num, group_idx, RegisterBase::BANK_IDX_DEFAULT);
    }

    /*!
     * \brief Alternate canLookupRegister with additional bank field
     * \note Separately defined so that lookup can be implemented
     * differently for banked and non-banked usage if required without a
     * performance penalty
     */
    bool canLookupRegister(RegisterBase::group_num_type group_num,
                           RegisterBase::group_idx_type group_idx,
                           RegisterBase::bank_idx_type bank_idx) const noexcept
    {
        return banks_.canLookupRegister(group_num, group_idx, bank_idx);
    }

    /*!
     * \brief Quickly looks up a register by its number and index with bounds
     * checking
     * \param group_num Group identifier number. Corresponds to
     * sparta::Register::Definition::group_num
     * \param group_idx Index within the given group. Corresponds to
     * sparta::Register::Definition::group_idx
     * \return regsiter requested if found, otherwise a nullptr if the group
     * number and index do not correspond to an actual register but happen
     * to be present in the internal lookup table. If parameters do not
     * correspond to an actual register in this set, may also cause a
     * segfault. Therefore, the result can (and should) be used without
     * null-checking.
     * \warning For performance, does not perform any bounds checking. Can
     * segfault if invalid parameters are given. Should be used in
     * conjunction with canLookupRegister (at simulation startup) to ensure
     * that lookups will succeed for all registers that will be accessed.
     * \throw Does not throw. May simply segfault.
     * \note Whether the result of this function will be a valid register
     *
     * The expected usage of this method is to use the result without null
     * checking since this method can cause fatal errors.
     * \code
     * rset->lookupRegister(gnum,gidx)->something()
     * \endcode
     * At simulator startup, the presence of all required registers can
     * be queried once with sparta::RegisterSet::canLookupRegister to ensure that
     * any register which may be be accessed during the course of simulation
     * will be available through this method.
     */
    RegisterBase *lookupRegister(RegisterBase::group_num_type group_num,
                                 RegisterBase::group_idx_type group_idx)
    {
        const auto &rm = banks_[RegisterBase::BANK_IDX_DEFAULT][group_num];
        const auto idx_count = rm.count(group_idx);
        if (idx_count==1) {
            sparta_assert(rm.at(group_idx) != nullptr);
        }
        return (idx_count==1 ? rm.at(group_idx) : nullptr);
    }

    /*!
     * \brief Alternate lookupRegister with additional bank field
     * \note Separately defined so that lookup can be implemented
     * differently for banked and non-banked usage if required without a
     * performance penalty from the extra parameter
     */
    RegisterBase *lookupRegister(RegisterBase::group_num_type group_num,
                                 RegisterBase::group_idx_type group_idx,
                                 RegisterBase::bank_idx_type bank_idx)
    {
        const auto &rm = banks_[bank_idx][group_num];
        const auto idx_count = rm.count(group_idx);
        if (idx_count==1) {
            sparta_assert(rm.at(group_idx) != nullptr);
        }
        return (idx_count==1 ? rm.at(group_idx) : nullptr);
    }

    /*!
     * \brief Gets a Register by group number and group index and throws an
     * exception if it cannot be found.
     * \param group_num Group identifier number. Corresponds to
     * sparta::Register::Definition::group_num
     * \param group_idx Index within the given group. Corresponds to
     * sparta::Register::Definition::group_idx
     * \return A non-nullptr Register. Guaranteed to return or throw
     * \throw SpartaExeption if the register cannot be found.
     * \note This is a safe alternative to the lookupRegister method with
     * bounds-checking, though with a performance penalty
     */
    RegisterBase *getRegister(RegisterBase::group_num_type group_num,
                              RegisterBase::group_idx_type group_idx)
    {
        return banks_.getRegister(group_num, group_idx, RegisterBase::BANK_IDX_DEFAULT);
    }

    /*!
     * \brief Alternate getRegister with additional bank field
     * \note Separately defined so that lookup can be implemented
     * differently for banked and non-banked usage if required without a
     * performance penalty
     */
    RegisterBase *getRegister(RegisterBase::group_num_type group_num,
                              RegisterBase::group_idx_type group_idx,
                              RegisterBase::bank_idx_type bank_idx)
    {
        return banks_.getRegister(group_num, group_idx, bank_idx);
    }

    /*!
     * \brief Gets the number of Registers in a group by its group num
     * \param group_num Number of group to get children from. If
     * GROUP_NUM_NONE, will always return 0.
     * \return Number of children in this group, including anonymous nodes.
     * Aliases do not affect this result. If no group with the given num was
     * specified, returns 0. This implies that the group was never created.
     */
    uint32_t getGroupSize(RegisterBase::group_num_type group_num) noexcept
    {
        return banks_.getGroupSize(group_num, RegisterBase::BANK_IDX_DEFAULT);
    }

    /*!
     * \brief Alternate getGroupSize with additional bank field
     * \note Separately defined so that lookup can be implemented
     * differently for banked and non-banked usage if required without a
     * performance penalty
     * \throw SpartaException if referring to an invalid bank_num
     */
    uint32_t getGroupSize(RegisterBase::group_num_type group_num,
                          RegisterBase::bank_idx_type bank_num)
    {
        return banks_.getGroupSize(group_num, bank_num);
    }

    /*!
     * \brief Determine the current bank for this register set based on the
     * context of time simulator.
     * \note This method has no guarantees about consistency. Each invocation
     * queries the simulator through the current-bank function (see
     * setGetCurrentBankFuncion)
     * \param group_num Group number of register to inspect
     * \param group_idx Group index of register to inspect
     * \param name_ptr Pointer to name of the register interned in
     * sparta::StringManager. Note that if this is nullptr, then only the
     * group_num and group_idx are available for lookup. If not-null, must
     * satisy sparta::StringManager::getStringManager().isInterned()
     */
    RegisterBase::bank_idx_type
    getCurrentBank(RegisterBase::group_num_type group_num,
                   RegisterBase::group_idx_type group_idx,
                   const std::string *name_ptr) const
    {
        if (cur_bank_fxn_ == nullptr) {
            return RegisterBase::BANK_IDX_DEFAULT;
        }
        return cur_bank_fxn_(group_num, group_idx, name_ptr);
    }

private:
    /*!
     * \brief React to a child registration
     * \param child TreeNode child that must be downcastable to a
     * sparta::Register. This is a borrowed reference - child is *not* copied.
     * child lifetime must exceed that of this RegisterSet instance.
     *
     * Overrides TreeNode::onAddingChild_
     */
    virtual void onAddingChild_(TreeNode *child) override
    {
        auto reg = dynamic_cast<RegisterBase *>(child);
        if (nullptr == reg) {
            throw SpartaException("Cannot add TreeNode child at ")
                << child << " which is not a Register to RegisterSet "
                << getLocation();
        }

        // Add register to regs_ list for tracking.
        regs_.push_back(reg);
    }

    /*!
     * \brief Create a new Register in this RegisterSet based on the given
     * Register::Definition
     * \param rdef Register Definition describing the new Register to create
     * \pre RegisterSet TreeNode cannot be finalized
     * \pre ArchData owned by this RegisterSet cannot be laid out
     * (getArchData().isLaidOut())
     * \post Adds to groups_ mapping table if this defintion specifies a
     * group num.
     *
     * This method is used internally to add Registers from the
     * Register::Definition array specified at construction.
     */
    template <typename RegisterT>
    void addRegister_(const RegisterBase::Definition *rdef)
    {
        sparta_assert(false == adata_.isLaidOut());// Cannot addRegister_ after the ArchData has been layed out
        sparta_assert(reg_proxies_.size() == 0); // Cannot addRegister_ once a proxy has been added
        auto r = new RegisterT(nullptr, *rdef, &adata_);

        // Attempt to insert the register into the bank table
        banks_.addRegister(r); // Throws if unable to add

        // Finally add register as child after it is validated
        this->addChild(r);
        owned_regs_.push_back(r);
    }


    /*!
     * \brief Add a new proxy with a given definition
     * \pre All Registers must have been added to this RegisterSet in order to
     * guarantee that all proxies can be validated against all possible
     * registers that they will proxy.
     * \pre At least one Register with a matching group number and group idx
     * must be present
     */
    void addProxy_(const RegisterProxyBase::Definition *pdef)
    {
        sparta_assert(pdef,
                          "Attempted to add a RegisterProxy to RegisterSet " << getLocation()
                          << " with a null proxy definition");

        // Check fields of the definition
        sparta_assert(pdef->name,
                          "Attempted to add a RegisterProxy to RegisterSet " << getLocation()
                          << " with a null proxy name");
        sparta_assert(pdef->desc,
                          "Attempted to add a RegisterProxy to RegisterSet " << getLocation()
                           << " named \"" << pdef->name << "\" with a null proxy description");
        sparta_assert(pdef->group_num != Register::GROUP_NUM_NONE,
                          "Attempted to add a RegisterProxy to RegisterSet " << getLocation()
                          << " named \"" << pdef->name << "\" with a group number of "
                          "GROUP_NUM_NONE. RegisterProxy definitions must have a valid group number");
        sparta_assert(pdef->group_idx != Register::GROUP_IDX_NONE,
                          "Attempted to add a RegisterProxy to RegisterSet " << getLocation()
                          << " named \"" << pdef->name << "\" with a group number of "
                          "GROUP_NUM_NONE. RegisterProxy definitions must have a valid group number");

        // Check ID conflicts with other proxies and registers
        for(auto& reg : regs_){
            sparta_assert(reg->getID() != pdef->id,
                              "Attempted to add a RegisterProxy to RegisterSet " << getLocation()
                              << " named \"" << pdef->name << "\" with an ID of " << pdef->id
                              << " which is already usd by Register " << reg);
        }

        // Check for a register with the same name
        auto reg_samename = getChildAs<RegisterBase>(pdef->name, false);
        sparta_assert(reg_samename == nullptr,
                          "Attempted to add a RegisterProxy to RegisterSet " << getLocation()
                           << " named \"" << pdef->name << "\" but there is already a normal "
                           "Register with the same name: " << reg_samename->getLocation());

        // Check for an existing proxy
        auto rpi = reg_proxies_.find(pdef->name);
        if(rpi != reg_proxies_.end()){
            throw SpartaException("A RegisterProxy in ") << getLocation()
                  << " named \"" << pdef->name << "\" already exists";
        }

        //! \todo Validate name
        //! \todo Validate description
        //! \todo Validate group name

        uint32_t matches = 0;
        for(uint32_t b = 0; b < banks_.getNumBanks(); b++){
            if(banks_.canLookupRegister(pdef->group_num, pdef->group_idx, b)){
                auto r = banks_.lookupRegister(pdef->group_num, pdef->group_idx, b);

                // Sanity check lookup
                sparta_assert(r);
                sparta_assert(r->getGroupNum() == pdef->group_num);
                sparta_assert(r->getGroupIdx() == pdef->group_idx);

                // Ensure that the group name matches
                sparta_assert(r->getGroupName() == pdef->group,
                                  "Attempted to add a RegisterProxy to RegisterSet "
                                  << getLocation() << " named \"" << pdef->name << "\" which matched "
                                  "the group number and group index with " << *r << " but the "
                                  "proxy had an incorrect group name \"" << pdef->group << "\"");

                matches++;
            }
        }

        // Ensure that one register matches
        sparta_assert(matches > 0,
                          "Attempted to add a RegisterProxy to RegisterSet "
                          << getLocation() << " named \"" << pdef->name << "\" with group num "
                          << pdef->group_num << " and group idx " << pdef->group_idx << " which did "
                          "not match any known registers");

        // Finally add the proxy
        auto rp = new RegisterProxy(*this,
                                    pdef->group_num,
                                    pdef->group_idx,
                                    StringManager::getStringManager().internString(pdef->name));
        reg_proxies_[pdef->name] = typename RegisterProxyMap::mapped_type(rp); // Takes ownership of memory
    }

    /*!
     * \brief ArchData that will hold all data for registers in this
     * RegisterSet
     */
    ArchData adata_;

    /*!
     * \brief All registers allocated by this set. These regs are deleted
     * at destruction of this RegisterSet
     */
    RegisterVector owned_regs_;

    /*!
     * \brief All registers contained by this set whether allocated by this
     * set or not (superset of owned_regs_)
     */
    RegisterVector regs_;

    /*!
     * \brief Table of banks for this registerset
     */
    RegisterBankTable<RegisterBase> banks_;

    /*!
     * \brief Map of register proxies keyed by name
     */
    RegisterProxyMap reg_proxies_;

    /*!
     * \brief Function for querying the current effective bank from a simulator
     */
    CurrentBankFunction cur_bank_fxn_;

}; // class RegisterSet

/*!
 * \brief Form a string representing this proxy
 */
inline std::string RegisterProxy::stringize() const
{
    std::stringstream ss;
    ss << "<RegisterProxy to ";
    if (reg_inv_) {
        ss << reg_inv_;
    } else {
        ss << "name: \"" << reg_name_ << "\" group: " << group_num_
           << " idx: " << group_idx_;
    }
    ss << " within " << rs_.getLocation() << ">";
    return ss.str();
}

inline RegisterBase *RegisterProxy::getCurrentRegister() const
{
    if (reg_inv_ != nullptr) {
        return reg_inv_;
    }

    auto bank_idx = rs_.getCurrentBank(group_num_, group_idx_, reg_name_);
    return rs_.getRegister(group_num_, group_idx_, bank_idx);
}

inline RegisterBase *RegisterProxy::tryGetCurrentRegister() const
{
    if (reg_inv_ != nullptr) {
        return reg_inv_;
    }

    auto bank_idx = rs_.getCurrentBank(group_num_, group_idx_, reg_name_);
    if (!rs_.canLookupRegister(group_num_, group_idx_, bank_idx)) {
        return nullptr;
    }
    return rs_.lookupRegister(group_num_, group_idx_, bank_idx);
}

inline RegisterSet &RegisterProxy::getContainingRegisterSet() const
{
    return rs_;
}

} // namespace sparta

//! ostream insertion operator for sparta::RegisterProxy
inline std::ostream &operator<<(std::ostream &o, const sparta::RegisterProxy &rp)
{
    o << rp.stringize();
    return o;
}

//! ostream insertion operator for sparta::RegisterProxy
inline std::ostream &operator<<(std::ostream &o, const sparta::RegisterProxy *rp)
{
    if (nullptr == rp) {
        o << "null";
    } else {
        o << rp->stringize();
    }
    return o;
}

//! ostream insertion operator for sparta::RegisterSet
inline std::ostream& operator<<(std::ostream& o, const sparta::RegisterSet &rs)
{
    o << rs.stringize();
    return o;
}

//! ostream insertion operator for sparta::RegisterSet
inline std::ostream &operator<<(std::ostream &o, const sparta::RegisterSet *rs)
{
    if(nullptr == rs){
        o << "null";
    }else{
        o << rs->stringize();
    }
    return o;
}

