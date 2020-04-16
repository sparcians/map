// <RegisterBankTable> -*- C++ -*-

#ifndef __REGISTER_BANK_TABLE_H__
#define __REGISTER_BANK_TABLE_H__

#include <iostream>
#include <unordered_map>

#include "sparta/functional/Register.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"


namespace sparta
{
/*!
 * \brief Container of register banks as a helper for RegisterSet. Instances
 * of this class will be owned by a single RegisterSet
 *
 * Contains a table of dimensions B (for each bank) and G (the number of
 * groups in the register set which owns that instance of this object/
 * The number of registers in a group can vary between groups.
 *
 * Banks and Groups effectively form a matrix of B,G dimensions.
 *
 * This structure is not intended to be dynamic. It can be built, but does
 * not expect changes to bank mappings at runtime
 */
template <typename RegisterT>
class RegisterBankTable
{
    /*!
     * \brief Bank idx threshold before a warning is printed about excessive
     * bank count. This number of banks can be used, but its probably a
     * mistake
     */
    static constexpr typename RegisterT::bank_idx_type WARN_MAX_BANK_IDX = 64;

    /*!
     * \brief Bank idx threshold before an exception is thrown about
     * excessive bank count. With this many banks, there is certainly an
     * error in usage (or caller is being extremely wasteful)
     */
    static constexpr typename RegisterT::bank_idx_type ERROR_MAX_BANK_IDX = 256;

public:

    /*!
     * \brief Vector of registers owned externally. Available for lookup
     * within a group by Register::Definition::group_idx_type
     */
    typedef std::unordered_map<Register::group_idx_type, RegisterT *> RegisterMap;
    typedef std::vector<RegisterT *> RegisterVector;

    /*!
     * \brief Vector of RegisterMap's used for lookup by numeric group
     * number of type Register::Definition::group_num_type
     */
    typedef std::vector<RegisterMap> GroupVector;

    /*!
     * \brief Vector of GroupVectors used for lookup by numeric bank
     * index of type Register::Definition::bank_idx_type
     */
    typedef std::vector<GroupVector> BankVector;

    /*!
     * \brief Constructor
     */
    RegisterBankTable() :
        num_groups_(0),
        num_regs_(0)
    {
        // Create an empty GroupVector because BANK_IDX_DEFAULT is
        // guaranteed to exist and is 0. If not 0, change this code to
        // ensure it exists
        static_assert(RegisterT::BANK_IDX_DEFAULT==0, "BANK_IDX_DEFAULT must be 0");
        banks_.push_back({});
    }

    /*!
     * \brief Destructor
     */
    virtual ~RegisterBankTable() = default;

    /*!
     * \brief Gets the total number of banks instantiated (even if they contain
     * have no actual registers accessible). Note that banks are created between
     * 0 and the highest bank index requested by a register
     */
    typename RegisterT::bank_idx_type getNumBanks() const
    {
        return banks_.size();
    }

    /*!
     * \brief Gets the number of register groups added to this table regardless
     * of banks.
     */
    typename RegisterT::group_idx_type getNumGroups() const
    {
        return num_groups_;
    }

    /*!
     * \brief Gets the number of registers in a group by its group num and bank
     * \param group_num Number of group to get children from. If GROUP_NUM_NONE,
     * will return 0.
     * \return Number of children in this group, including anonymous nodes.
     * Aliases do not affect this result. If no group with the given num was
     * specified, returns 0. This implies that the group was never created.
     * \throw SpartaException if referring to an invalid bank_num
     */
    uint32_t getGroupSize(typename RegisterT::group_num_type group_num,
                          typename RegisterT::bank_idx_type bank_num)
    {
        if(group_num == RegisterT::GROUP_NUM_NONE){
            return 0;
        }
        if(bank_num >= banks_.size()){
            throw SpartaException("Cannot get group size of bank ")
                << bank_num << " group " << group_num << " because there are only "
                << banks_.size() << " banks in bank table " << stringize();
        }
        if(group_num >= banks_[bank_num].size()){
            return 0;
            //throw SpartaException("Cannot get group size of bank ")
            //    << bank_num << " group " << group_num << " because there are only "
            //    << banks_[bank_num].size() << " groups in bank table " << stringize();
        }
        return banks_[bank_num][group_num].size();
    }

    GroupVector& operator[](typename RegisterT::bank_idx_type bank_idx)
    {
        return banks_[bank_idx];
    }

    GroupVector& at(typename RegisterT::bank_idx_type bank_idx)
    {
        return banks_.at(bank_idx);
    }

    /*!
     * \brief Returns number of registers in this table. This excludes any
     * registers added having no group number.
     */
    uint32_t getNumRegisters() const {
        return num_regs_;
    }

    /*!
     * \brief Sets the minimum bank index for this register set, overriding
     * the default of BANK_IDX_DEFAULT.
     * \param min_idx Minimum bank index to be accessible. This is an index,
     * so the minimum number of banks required will be \a min_idx+1
     * \note This can only increase the number of banks currently represented
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
    void setMinimumBankIndex(Register::bank_idx_type min_idx) {
        extendBanks_(min_idx + 1);
    }

    /*!
     * \brief Adds a register to this table unless it is not a member of a group
     * (See Register::getGroupNum
     * \param r Register to add to the banking table
     * \post Bank table will have a bank set capable of containing all
     * entries in the register bank membership
     * \post getNumRegisters will increase unless this register's group_num is
     * Register::GROUP_NUM_NONE
     * \throw SpartaException if r is nullptr, the definition of r has a
     * bank_membership field with an index at or exceeding
     * ERROR_MAX_BANK_IDX, or on index in the bank_membership along with the
     * group number and group index already point to an existing register. If
     * the bank_membership field of the register definition is empty,
     * any register existing in any bank having the same group and index
     * will cause an exception. If register has a group_num of
     * Register::GROUP_NUM_NONE and a non-empty bank membership set, throws
     */
    void addRegister(RegisterT *r)
    {
        sparta_assert(r != nullptr);

        const auto &rdef = r->getDefinition();

        if(rdef.group_num == RegisterT::GROUP_NUM_NONE){
            if(rdef.bank_membership.size() != 0){
                throw SpartaException("A register has no group number so it cannot be looked up "
                    "through a bank, but but does have bank membership information. This is "
                    "probably a mistake in one of these two fields. Error while adding unbanked "
                    "register ") << *r;
            }
        }else{

            if(rdef.group_num > num_groups_ && rdef.group_num >= 300){
                //! \todo Use logger
                std::cerr << "WARNING: Register " << r->getLocation()
                          << " Group num is very large: " << rdef.group_num
                          << ". This requires a vector to be allocated of this size and "
                          "probably wastes memory";
            }

            if(r->isBanked() == false){
                // 1 bank is expected (from construction) so that this register can
                // be tested against other registers in the banks without needing to
                // compare against the unbanked_regs_ list
                sparta_assert(banks_.size() > 0, "1 or more banks expected before any addRegister calls");

                // Unbanked register
                // Check for collisions in all existing banks
                for(size_t bank_idx = 0; bank_idx < banks_.size(); ++bank_idx){
                    if(canLookupRegister(rdef.group_num, rdef.group_idx, bank_idx)){
                        throw SpartaException("A regsiter already exists in bank ") << bank_idx
                            << " with group num " << rdef.group_num << " and group idx "
                            << rdef.group_idx << ". Error while adding unbanked register: " << *r;
                    }
                }

                // Safely add the register to the table
                unbanked_regs_.push_back(r);

                for(GroupVector& gv : banks_){
                    insertRegisterInBank_(r, gv); // Should never throw
                }
            }else{
                // Banked register
                // Check for collisions in existing banks for which
                // this register is accessible
                typename RegisterT::bank_idx_type max_bank_idx = 0;
                for(typename RegisterT::bank_idx_type bank_idx : rdef.bank_membership){
                    max_bank_idx = std::max(max_bank_idx, bank_idx);
                    if(bank_idx < banks_.size()){
                        // Bank currently exists, check for collisions
                        if(canLookupRegister(rdef.group_num, rdef.group_idx, bank_idx)){
                            throw SpartaException("A regsiter already exists in bank ") << bank_idx
                                << " with group num " << rdef.group_num << " and group idx "
                                << rdef.group_idx << ". Error while adding banked register: " << *r;
                        }
                    }
                }

                if(max_bank_idx >= WARN_MAX_BANK_IDX){
                    //! \todo Use logger
                    std::cerr << "WARNING: Register " << r->getLocation()
                              << " bank membership number contains a large value: " << max_bank_idx
                              << ". This requires a vector to be allocated of this size and "
                                 "probably wastes memory";
                }
                if(max_bank_idx >= ERROR_MAX_BANK_IDX){
                    //! \todo Use logger
                    throw SpartaException("Register ") << r->getLocation()
                        << " bank membership number contains a very large value: " << max_bank_idx
                        << ". This requires a vector to be allocated of this size and is likely a "
                        "mistake. If not, increase RegisterBankTable::ERROR_MAX_BANK_IDX";
                }

                // Check for collisions with unbanked registers
                for(auto ubr : unbanked_regs_){
                    if(ubr->getGroupNum() == r->getGroupNum()
                       && ubr->getGroupIdx() == r->getGroupIdx()){
                        throw SpartaException("An unbanked regsiter already exists in this set with ")
                            << "group num " << r->getGroupNum() << " and group idx "
                            << r->getGroupIdx() << ". Error while adding banked register: " << *r;
                    }
                }

                // Extend all banks to fit the max bank index of this register
                extendBanks_(max_bank_idx + 1);

                // Insert this register into each bank of which it is a member.
                // banks_ will be large enough to contain each bank idx
                for(auto bank_idx : rdef.bank_membership){
                    GroupVector& bank = banks_[bank_idx];
                    insertRegisterInBank_(r, bank); // Should never throw
                }
            }

            num_groups_ = std::max(num_groups_, rdef.group_num+1);
            ++num_regs_;
        }
    }

    bool canLookupRegister(typename RegisterT::group_num_type group_num,
                           typename RegisterT::group_idx_type group_idx,
                           typename RegisterT::bank_idx_type bank_idx) const noexcept
    {
        return banks_.size() > bank_idx
            && banks_[bank_idx].size() > group_num
            && banks_[bank_idx][group_num].count(group_idx) != 0;
    }

    RegisterT *lookupRegister(typename RegisterT::group_num_type group_num,
                              typename RegisterT::group_idx_type group_idx,
                              typename RegisterT::bank_idx_type bank_idx)
    {
        const auto &rm = banks_[bank_idx][group_num];
        const auto idx_count = rm.count(group_idx);
        if (idx_count==1) {
            sparta_assert(rm.at(group_idx) != nullptr);
        }
        return (idx_count==1 ? rm.at(group_idx) : nullptr);
    }

    const RegisterT *lookupRegister(typename RegisterT::group_num_type group_num,
                                    typename RegisterT::group_idx_type group_idx,
                                    typename RegisterT::bank_idx_type bank_idx) const
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
    RegisterT *getRegister(typename RegisterT::group_num_type group_num,
                           typename RegisterT::group_idx_type group_idx,
                           typename RegisterT::bank_idx_type bank_idx) {
        if(__builtin_expect(banks_.size() <= bank_idx, 0)){
            throw SpartaException("Register set ")
                << stringize() << " has no bank_idx " << bank_idx;
        }
        GroupVector& gv = banks_[bank_idx];
        if(__builtin_expect(gv.size() <= group_num, 0)){
            throw SpartaException("Register set ")
                << stringize() << " has no group " << group_num
                << " in bank " << bank_idx;
        }
        RegisterMap& rm = gv[group_num];
        if(__builtin_expect(rm.size() == 0, 0)){
            // Empty register map for this group - it does not exist
            throw SpartaException("Register set ")
                << stringize() << " has no group " << group_num;
        }

        if(__builtin_expect(rm.count(group_idx) == 0, 0)){
            // Null register at this index
            throw SpartaException("Register set ")
                << stringize() << " has no register with idx " << group_idx
                << " in group " << group_num;
        }

        sparta_assert(rm.at(group_idx) != nullptr);
        return rm[group_idx];
    }

    // Overload of TreeNode::stringize
    virtual std::string stringize(bool pretty=false) const {
        (void) pretty;
        std::stringstream ss;
        //ss << '<' << getLocation() << ' ' << regs_.size() << " regs>";
        ss << "<RegisterSet bank table: " << banks_.size() << " banks, "
           << num_regs_ << " phy regs>";
        return ss.str();
    }

    /*!
     * \brief Dump this register bank table to an out stream. Banks will be
     * columns and group num/id will be rows
     */
    void dump(std::ostream& out, bool detailed=false) const {
        (void) detailed;

        const uint32_t GROUP_NAME_WIDTH = 5;
        const uint32_t GROUP_NUM_WIDTH = 4;
        const uint32_t GROUP_IDX_WIDTH = 4;
        const uint32_t COL_WIDTH = 8;

        typename RegisterT::group_num_type group_num_max = 0;

        // Header of bank columns
        out << "          banks->|";
        for(size_t bank_idx = 0; bank_idx != banks_.size(); ++bank_idx){
            out << std::setw(COL_WIDTH) << bank_idx << "|";

            // Determine which bank has the largest group
            decltype(group_num_max) num_groups = banks_[bank_idx].size();
            if(num_groups > group_num_max){
                group_num_max = num_groups - 1;
            }

        }
        out << std::endl;

        // Header for groups and cross-bar
        out << "     group |index|";
        for(size_t bank_idx = 0; bank_idx != banks_.size(); ++bank_idx){
            out << "--------|"; // Column width
        }

        out << std::endl;

        // Rows
        for(typename RegisterT::group_num_type group_num = 0; group_num <= group_num_max; ++group_num) {
            // Determine the largest group_idx in this group by iterating all
            // banks and looking for the largest RegisterMap key in that bank
            typename RegisterT::group_idx_type group_idx_max = 0;
            for(size_t bank_idx = 0; bank_idx != banks_.size(); ++bank_idx){
                if(banks_[bank_idx].size() > group_num){
                    // Bank contains this register group
                    const auto & bank = banks_[bank_idx];
                    for (auto &it : bank[group_num]) {
                        group_idx_max = std::max(group_idx_max, it.first);
                    }
                }
            }

            // Row for each item in the group
            bool wrote_group = false; // Has this group number been written yet
            for(typename RegisterT::group_idx_type group_idx = 0; group_idx <= group_idx_max; ++group_idx) {

                // Check that there at least 1 register in any bank for this group and index
                bool has_reg = false;
                std::string group_name; // Name of this group (extracted below)
                std::vector<std::string> names;
                for(size_t bank_idx = 0; bank_idx != banks_.size(); ++bank_idx){
                    if(canLookupRegister(group_num, group_idx, bank_idx)){
                        if(has_reg == false){
                            group_name = lookupRegister(group_num, group_idx, bank_idx)->getGroupName();
                        }
                        has_reg = true;
                        const auto r = lookupRegister(group_num, group_idx, bank_idx);
                        names.push_back(r->getName());
                    }else{
                        names.push_back("");
                    }
                }

                if(has_reg){
                    // Display this group/index row for each bank
                    if(wrote_group){
                        writeNChars(out, GROUP_NAME_WIDTH + 1 + GROUP_NUM_WIDTH);
                    }else{
                        wrote_group = true;
                        out << std::setw(GROUP_NAME_WIDTH) << group_name << ' '
                            << std::setw(GROUP_NUM_WIDTH) << group_num;
                    }
                    out << " |" << std::setw(GROUP_IDX_WIDTH) << group_idx << " |";

                    // Column for each bank
                    for(const std::string& val : names){
                        out << std::setw(COL_WIDTH) << val << "|";
                    }

                    out << std::endl;
                }

            }
        }

    }

protected:

    /*!
     * \brief Extends the banks_ vector to contain num_banks entries
     * \param num_banks number of banks that must be contained in extendBanks
     * \post banks_.size() will be >= num_banks
     * \post Any Register in unbanked_regs_ will be accessible in the added
     * banks (presumably it was already accessible in the original banks)
     */
    void extendBanks_(typename RegisterT::bank_idx_type num_banks)
    {
        // Add missing banks if required
        while(num_banks > banks_.size()){
            banks_.push_back({});
            GroupVector& gv = banks_.back();
            for(auto r : unbanked_regs_){
                insertRegisterInBank_(r, gv);
            }
        }
    }

    /*!
     * \brief Insert a register in a specific bank
     * \param r Register to insert
     * \param bank GroupVector of the bank into which this register will be
     * inserted.
     * \post Increases the group size of the given bank to contain the register
     * \post Increases the register map size of the group within this bank
     * that contains the register
     * \throw Must not throw. Check that the register can be added first
     */
    void insertRegisterInBank_(RegisterT *r, GroupVector& bank)
    {
        sparta_assert(r != nullptr);
        const auto rdef = r->getDefinition();
        while(rdef.group_num >= bank.size()){
            bank.push_back(RegisterMap());
        }
        RegisterMap& rm = bank[rdef.group_num];
        sparta_assert(rm.count(rdef.group_idx)==0);
        rm.insert(std::make_pair(rdef.group_idx, r));
        return;
    }

private:

    /*!
     * \brief Vector of GroupVectors indexed by numeric bank index for fast
     * lookup of banks followed by lookup by group num then register index.
     *
     * Lookups can be performed as in banks_[bank_idx][group_num][group_idx]
     * All banks contain the same number of groups. However, all groups
     * can contain any number of registers and a 0-count group means that
     * the group does not exist
     *
     * This vector and its contents are to be treated as append-only
     */
    BankVector banks_;

    /*!
     * \brief Unbanked registers (where definition's banked_membership is empty)
     */
    std::vector<RegisterT *> unbanked_regs_;

    /*!
     * \brief Number of groups represented (even if not accessible in every
     * bank)
     */
    typename RegisterT::group_num_type num_groups_;

    /*!
     * \brief Number of physical registers (i.e. number of addRegister calls
     */
    uint32_t num_regs_;
};

} // namespace sparta

// __REGISTER_BANK_TABLE_H__
#endif
