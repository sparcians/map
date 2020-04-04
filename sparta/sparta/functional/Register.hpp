// <Register> -*- C++ -*-

#ifndef __REGISTER_H__
#define __REGISTER_H__

#include <iostream>
#include <ios>
#include <iomanip>
#include <sstream>
#include <math.h>
#include <memory>

#include <boost/static_assert.hpp>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/DataView.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/log/NotificationSource.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/BitArray.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/utils/ValidValue.hpp"

namespace sparta
{

/*!
 * \brief Base class to represents an architected register of any size that
 * is a power of 2 and greater than 0 with a ceiling specified
 *
 * \note Maximum register size is constrained by the ArchData instance where
 * the register value resides. This is a property of the sparta::RegisterSet
 * which owns this sparta::Register.
 *
 * A sparta::Register may contain any number of sparta::Register::Field
 * instances refering to bit ranges within the Register. These ranges may
 * overlap within the register, have no alignment restrictions, and must
 * be greater and must be at least 1 bit wide. See sparta::Register::Field
 * for the maximum size constraint.
 *
 * Constructed based on a parent sparta::TreeNode, which should be a
 * sparta::RegisterSet, as well as a sparta::Register::Definition describing
 * the structure of the register and a sparta::ArchData where the Register
 * data will reside. Generally, Registers are only constructed
 * (and then owned) by sparta::RegisterSet instances.
 *
 * sparta::Register is a subclass of TreeNode, which allows a name, parent,
 * aliases, and group information. A sparta::RegisterSet parent may also use
 * TreeNode get/find facilities for obtaining pointers to children by name
 *
 * Register itself is not meant to be overloaded. It is important to keep
 * access methods as inline templated methods for performance. Making them
 * virtual would negatively impact performance. This class comprises little
 * other functionality; only some const attributes and helper functions.
 *
 * Contains NotificationSource child nodes for types
 * sparta::Register::ReadAccess and sparta::Register::PostWriteAccess.
 * See getPostWriteNotificationSource and getReadNotificationSource.
 *
 * \todo Add register notifications between compound registers
 */
class RegisterBase : public TreeNode
{
private:
    using BitArray = utils::BitArray;

public:
    class Field;

    /*!
     * \brief Identifier to distinguish from other registers in the same
     * RegisterSet
     */
    typedef DataView::ident_type ident_type;

    /*!
     * \brief Index of read/write access within register
     */
    typedef DataView::index_type index_type;

    /*!
     * \brief Size of register and bit or byte offset within register data
     */
    typedef DataView::offset_type size_type;

    /*!
     * \brief Numeric group identifier for register lookup
     */
    typedef uint32_t group_num_type;

    /*!
     * \brief TreeNode group index
     */
    typedef TreeNode::group_idx_type group_idx_type;

    /*!
     * \brief Numeric bank identifier for bank lookup
     * \note Must be unsigned
     */
    typedef uint32_t bank_idx_type;

    /*!
     * \brief Vector of Register Aliases
     */
    typedef std::vector<const char*> AliasVector;

    /*!
     * \brief Vector of Register Fields
     */
    typedef std::vector<Field*> FieldVector;

    /*!
     * \brief Register read callback
     */
    typedef std::function<sparta::utils::ValidValue<uint64_t>(RegisterBase*)> register_read_callback_type;

    /*!
     * \brief Register write callback
     */
    typedef std::function<bool(RegisterBase*, uint64_t)> register_write_callback_type;

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Default index for bank when no bank is specified. A bank
     * having this index will always exist.
     */
    static constexpr bank_idx_type BANK_IDX_DEFAULT = 0;

    /*!
     * \brief Register Field with masked access to owning register
     *
     * Constructed with a Field::Definition, performs accesses on parent
     * register to implement its read and write methods.
     */
    class Field : public TreeNode
    {
        utils::BitArray computeFieldMask_(uint32_t start, uint32_t end, uint32_t reg_size)
        {
            const auto num_ones = start - end + 1;
            // For 31-0:
            // max() & (max() >> ((8 * 8) - 31))
            return utils::BitArray(((std::numeric_limits<uint64_t>::max()
                                     & (std::numeric_limits<uint64_t>::max() >> ((sizeof(uint64_t) * CHAR_BIT) - num_ones)))), reg_size) << end;
        }

    public:

        /*!
         * \brief Type used for bitfield access.
         */
        typedef uint64_t access_type;

        /*!
         * \brief Maximum number of bits allowed in a field.
         */
        static const size_type MAX_FIELD_BITS = sizeof(access_type) * 8;

        /*!
         * \brief Field Definition structure
         * \todo Add reserved field (or other missing attributes)
         */
        struct Definition
        {
            /*!
             * \brief Allow default constructor
             */
            Definition() = delete;

            /*!
             * \brief Limited constructor for backward compatibility
             * \note read_only is initialized to false
             */
            Definition(const char* _name,
                       const char* _desc,
                       size_type _low_bit,
                       size_type _high_bit) :
                name(_name),
                desc(_desc),
                low_bit(_low_bit),
                high_bit(_high_bit),
                read_only(false)
            {;}

            Definition(const char* _name,
                       const char* _desc,
                       size_type _low_bit,
                       size_type _high_bit,
                       bool _read_only) :
                name(_name),
                desc(_desc),
                low_bit(_low_bit),
                high_bit(_high_bit),
                read_only(_read_only)
            {;}

            const char* name;   //!< Name - Must ahere to TreeNode::validateName. Must not be NULL.
            const char* desc;   //!< Description. Must NOT be NULL
            size_type low_bit;  //!< Low bit (inclusive)
            size_type high_bit; //!< High bit (inclusive). Must be >= low_bit. (high_bit-low_bit) must be < MAX_FIELD_BITS. Larger fields are not supported
            bool read_only;     //!< Is this a read-only field
        };

        /*!
         * \brief Field Constructor
         * \param reg Register parent
         * \param def Field definition. Must not be null
         *
         * Constructs Field from the given definition
         */
        Field(RegisterBase &reg, const Definition &def) :
            TreeNode(NULL_TO_EMPTY_STR(def.name), TreeNode::GROUP_NAME_NONE,
                     TreeNode::GROUP_IDX_NONE, NULL_TO_EMPTY_STR(def.desc)),
            reg_(reg),
            def_(def),
            reg_size_(reg_.getNumBytes()),
            field_mask_(computeFieldMask_(def.high_bit, def.low_bit, reg_.getNumBytes()))
        {
            setExpectedParent_(&reg);

            sparta_assert(def.name != 0, "Register::Field::Definition::name cannot be empty");

            if(def.high_bit < def.low_bit){
                throw SpartaException("Register Field ")
                    << getLocation() << " definition contains high bit (" << def.high_bit
                    << ") less than a low bit (" << def.low_bit << ")";
            }

            if(def.low_bit >= reg.getNumBits()){
                throw SpartaException("Register Field ")
                    << getLocation() << " definition contains a low bit (" << def.low_bit
                    << ") greater than or equal to the number of bits in the register ("
                    << reg.getNumBits() << ")";
            }
            if(def.high_bit >= reg.getNumBits()){
                throw SpartaException("Register Field ")
                    << getLocation() << " definition contains a high bit (" << def.high_bit
                    << ") greater than or equal to the number of bits in the register ("
                    << reg.getNumBits() << ")";
            }

            if(getNumBits() > MAX_FIELD_BITS){
                throw SpartaException("Cannot currently support more than ")
                    << (size_type)MAX_FIELD_BITS << "bit-wide fields. "
                    << "Problem with field \"" << getLocation() << "\"";
            }

            // Add self as child after successful initialization
            reg.addChild(this);
        }

        //! \name Access methods
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Read the field
         * \todo Support indexes for reading more than \a access_type elements
         * \todo Support reading with different integer sizes
         */
        access_type read()
        {
            return ((readBitArray_() & field_mask_) >> getLowBit()).getValue<access_type>();
        }

        /*!
         * \brief Peeks the field
         * \todo Support indexes for reading more than \a access_type elements
         * \todo Support reading with different integer sizes
         */
        access_type peek() const
        {
            return ((peekBitArray_() & field_mask_) >> getLowBit()).getValue<access_type>();
        }

        /*!
         * \brief Write the field
         * \todo Support indexes for reading more than \a access_type elements
         * \todo Support reading with different integer sizes
         * \note read-only fields are supported through the register access
         */
        void write(access_type t)
        {
            write_(newRegisterValue_(t));
        }

        /*!
         * \brief Poke the field
         * \todo Support indexes for reading more than \a access_type elements
         * \todo Support reading with different integer sizes
         * \note read-only fields are applied within the register write called
         * by this method
         */
        void poke(access_type t)
        {
            poke_(newRegisterValue_(t));
        }

        /*!
         * \brief Poke the field without any read-only mask being applied
         */
        void pokeUnmasked(access_type t)
        {
            pokeUnmasked_(newRegisterValue_(t));
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Attributes
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Returns the index of the low bit (inclusive) in this
         * field
         * \return low bit index. This number will be <= getHighBit()
         */
        size_type getLowBit() const { return def_.low_bit; }

        /*!
         * \brief Returns the index of the high bit (inclusive) in this
         * field
         * \return low bit index. This number will be >= getLowBit()
         */
        size_type getHighBit() const { return def_.high_bit; }

        /*!
         * \brief Returns true if this field is marked read-only
         */
        bool isReadOnly() const { return def_.read_only; }

        /*!
         * \brief Gets number of bits in this field
         */
        size_type getNumBits() const
        {
            return getHighBit() - getLowBit() + 1;
        }

        /*!
         * \brief Gets the Definition with which this Field was constructed
         */
        const Definition& getDefinition() const {
            return def_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        // Override from TreeNode
        virtual std::string stringize(bool pretty=false) const override{
            (void) pretty;
            std::stringstream ss;
            ss << '<' << getLocation() << " [" << def_.low_bit << "-"
               << def_.high_bit << "] " << getNumBits();
            ss << " bits LE:0x" << std::hex << peek();
            if(def_.read_only){
                ss << " READ-ONLY";
            }
            ss << '>';
            return ss.str();
        }

    private:
        utils::BitArray readBitArray_() const
        {
            std::vector<uint8_t> value(reg_size_);
            reg_.read(value.data(), reg_size_, 0);
            return utils::BitArray(value.data(), reg_size_);
        }

        utils::BitArray peekBitArray_() const
        {
            std::vector<uint8_t> value(reg_size_);
            reg_.peek(value.data(), reg_size_, 0);
            return utils::BitArray(value.data(), reg_size_);
        }

        void write_(const utils::BitArray &value)
        {
            reg_.write(value.getValue(), value.getSize(), 0);
        }

        void poke_(const utils::BitArray &value)
        {
            reg_.poke(value.getValue(), value.getSize(), 0);
        }

        void pokeUnmasked_(const utils::BitArray &value)
        {
            reg_.pokeUnmasked(value.getValue(), value.getSize(), 0);
        }

        utils::BitArray newRegisterValue_(access_type value) const
        {
            const auto old_register_value  = peekBitArray_();
            const auto field_value_to_be_written_shifted = utils::BitArray(value, reg_size_) << getLowBit();

            // Check to see if the number of bits being written to the
            // field is larger than the field itself.
            sparta_assert((field_value_to_be_written_shifted & ~field_mask_) == utils::BitArray(access_type(0), reg_size_),
                          "Value of " << value <<  " too large for bit field "
                          << getLocation() << " of size " << getNumBits());

            return (old_register_value & ~field_mask_) | field_value_to_be_written_shifted;
        }

        /*!
         * \brief Register into which Field is reading
         */
        RegisterBase &reg_;

        /*!
         * \brief Field definition specified at construction
         */
        const Definition &def_;

        /*!
         * Size of the register this field belongs to in bytes
         */
        const RegisterBase::size_type reg_size_ = 0;

        /*!
         * Used to mask out the bits in the register of this field --
         * the 'not' bits of the register.
         */
        const utils::BitArray field_mask_;

    }; // class Field

    /*!
     * \brief Structure containing data for a Register pre- or post-read
     * notification
     *
     * The data in this structure is only guaranteed to be valid within
     * a notification callback
     */
    struct ReadAccess
    {
        ReadAccess() = delete;

        ReadAccess(const RegisterBase *_reg, const DataView *_value_dview)
        : reg(_reg), value(_value_dview)
        {
        }

        /*!
         * \brief Register on which the read access took place
         */
        const RegisterBase *const reg;

        /*!
         * \brief Value just read from \a reg. This value can also be
         * retrieved through \a reg.
         */
        const DataView *const value;
    };

    /*!
     * \brief Notification type for Register read accesses
     */
    typedef NotificationSource<ReadAccess> ReadNotiSrc;

    /*!
     * \brief Structure containing data for a Register post-write notification
     *
     * The data in this structure is only guaranteed to be valid within
     * a notification callback
     */
    struct PostWriteAccess
    {
        PostWriteAccess() = delete;

        PostWriteAccess(const RegisterBase *_reg,
                        const DataView *_prior_dview,
                        const DataView *_final_dview)
        : reg(_reg), prior(_prior_dview), final(_final_dview)
        {
        }

        /*!
         * \brief Register on which the write access took place
         */
        const RegisterBase *const reg;

        /*!
         * \brief Value of \a reg prior to this write
         */
        const DataView *const prior;

        /*!
         * \brief Value of \a reg after write (after masking and/or actions
         * such as clear-on-write are applied). Final can also be retrieved
         * through \a reg
         */
        const DataView *const final;
    };

    /*!
     * \brief Notification type for Register write accesses
     */
    typedef NotificationSource<PostWriteAccess> PostWriteNotiSrc;

    /*!
     * \brief Describes an architected Register
     *
     * Can be constructed with initializer list as in:
     *
     * \code
     * const Register::group_num_type GROUP_ASPR = 0;
     * const Register::group_num_type GROUP_GPR  = 1;
     *
     * sparta::Register::Field::Definition aspr_fields[] =
     *     ;
     *
     * sparta::Register::Definition regs[] =
     *     { {0, "aspr", 0, GROUP_ASPR, "aspr", 0, "This is ASPR 1", 4,
     *                    { { "N",  "Negative condition flag", 31, 31 },
     *                    { "Z",  "Zero condition flag", 30, 30 },
     *                    { "C",  "Carry condition flag", 29, 29 },
     *                    { "V",  "Overflow condition flag", 28, 28 },
     *                    { "Q",  "Overflow or saturation flag", 27, 27 },
     *                    { "GE", "Greater than or equal flags", 23, 26 } },
     *           nullptr, Register::INVALID_ID, 0},
     *       {0, "r1",  1, GROUP_GPR,  "gpr",  1, "This is GPR 1",  {} , {BANK_IDX_DEFAULT}, nullptr, Register::INVALID_ID, 0, nullptr},
     *       sparta::Register::DEFINITION_END };
     * \endcode
     *
     * \todo Add omitted fields:
     * \li (address)
     * \li (por value or functor)
     * \li (r/w masks)
     * \li (access/privilege levels)
     */
    struct Definition
    {
        /*!
         * \brief ID. Must be unique within a register set
         */
        const ident_type id;

        /*!
         * \brief String identifier for this register which
         * distinguishes it from its neighbors. Must adhere to
         * TreeNode::validateName. Must NOT be NULL.
         */
        const char*              name;

        /*!
         * \brief Numeric identifer for the group to which this register
         * belongs. This identifies the group for fast lookups in
         * a register set by group. It is up to the user of a
         * RegisterSet to give meaning to group numbers. Group
         * numbers must consistently map to the same group string
         * within a RegisterSet. If set to anything but
         * GROUP_NUM_NONE, group must be set to something other
         * than sparta::TreeNode::GROUP_NAME_NONE. Internally, a
         * lookup-vector is built for fast queries, so low
         * numbers should be used.
         */
        const group_num_type     group_num;

        /*!
         * \brief String name of group in which this register resides
         * (e.g. gpr). Must NOT be NULL. See
         * sparta::TreeNode::TreeNode. It is up to the user of a
         * RegisterSet to give meaning to group names. All
         * Definitions in the same RegisterSet must contain
         * consistent (group_num:group) mappings.
         */
        const char*              group;

        /*!
         * \brief Index of register within group. See
         * sparta::TreeNode::TreeNode. Internally, a lookup-vector
         * is built for fast queries, so low indexes should be
         * used.
         */
        const group_idx_type     group_idx;

        /*!
         * \brief Description. Must NOT be NULL.
         */
        const char*              desc;

        /*!
         * \brief Size of this register in bytes. Non-byte multiples are
         * not supported
         */
        const size_type          bytes;

        /*!
         * \brief Vector of field definitions.  Use like so for empty fields:
         *  { name, group_num,..., bytes, {}, aliases ... };
         * and like so for values
         *  { name, group_num,..., bytes, { {"afield","right here",0,1} }, aliases ... };
         */
        const std::vector<Field::Definition> fields;

        /*!
         * \brief Vector of bank indexes of banks in which this register is
         * accessible.
         * \note These indices are not necessarily equal to the simulated
         * architecture's execution modes (or whatever determines which
         * bank to access).
         * \note An empty set here means that the register is "unbanked" -
         * and causes the register to be accessible in every bank.
         * \note The total number of banks in existance in the RegisterSet
         * containing this register is dictated by the register having the
         * highest index in its definition's bank_membership field (or 0 if
         * no registers have any bank indices in their bank_membership
         * fields). This can be overridden by
         * RegisterSet::setMinimumBankIndex.
         * \note Do not duplicate indices in this list
         * \note An index in this list must not collide with any other
         * register in the same register set having the same group number
         * and group index
         * In a non-banked system, leave this vector empty or add a single
         * element containing BANK_IDX_DEFAULT.
         * The indices in this vector needn't be defined ahead of time.
         * The RegisterSet model will build its representation up as it
         * encounters new bank indexes in this field.
         * \warning Banking is implemented using using a dense array, so
         * using large values can waste lost of host memory.
          */
        const std::vector<bank_idx_type> bank_membership;

        /*!
         * \brief Null-terminated array of of char*s (e.g. {"a", "b",
         * 0}). If there are no aliases, this may be 0.
         */
        const char**             aliases;

        /*!
         * \brief ID of register of which this is a subset. If
         * INVALID_ID, has no effect.
         */
        const ident_type         subset_of;

        /*!
         * \brief Offset (in Bytes) into regster of which this is a subset.
         * subset_offset+(this->bytes) must be <= the size (bytes) of the
         * register of which this is a subset (this->subset_of). This field has
         * no effect if subset_of=INVALID_ID.
         */
        const size_type          subset_offset;

        /*!
         * \brief Initial value of this register
         */
        const unsigned char*     initial_value;

        /*!
         * \brief Register hint flags. The flags are not part of sparta but
         * should be defined by the model.
         */
        typedef uint16_t         HintsT;
        const HintsT             hints;

         /*!
         * \brief Register Domain. The flags are not part of sparta but
         * should be defined by the model.
         */
        typedef uint16_t         RegDomainT;
        const RegDomainT         regdomain;
    };

    //! Represents an invalid Register ID
    static constexpr ident_type INVALID_ID = DataView::INVALID_ID;

    //! Represents no group much like sparta::TreeNode::GROUP_NAME_NONE
    static constexpr group_num_type GROUP_NUM_NONE = ~(group_num_type)0;

    //! Entry indicating the end of a sparta::Register::Definition array
    static const Definition DEFINITION_END;

    //! Allow Fields to access register internals (e.g. dview_)
    friend class Field;

    //! \name Construction & Initialization
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Register constructor
     * \param parent parent node
     * \param def array of sparta::Register::Definition objects, terminated
     * with a Definition having a NULL name field. See DEFINITION_END.
     * Also see Register::Field::Definition fields for details
     * \param adata ArchData in which to store this register's valu
     * This array and its content must exist for the duration of this
     * Register since a reference will be kept to the Definition and any
     * Fields or aliases within that definition.
     * \post Computes a write mask for this register with which all writes will
     * be filtered before being written to memory.
     * Note that bits in the register not covered by a field are considered
     * writable and readable. Overlapping fields with conflicing read-only flags
     * result the register having read-only bits matching all fields flagged
     * as read-only. That is, bits default to read-write, but once a field with
     * a read-only flag is encountered, the bits it comprises are permanently
     * marked as read-only regardless of any read-write fields containing those
     * bits
     */
    RegisterBase(TreeNode *parent, const Definition *def)
        : TreeNode(nullptr,
                   NULL_TO_EMPTY_STR(def->name),
                   NULL_TO_EMPTY_STR(def->group),
                   def->group_idx,
                   NULL_TO_EMPTY_STR(def->desc),
                   false),
          def_(def),
          bits_(def->bytes * 8),
          mask_(computeWriteMask_(def)),
          post_write_noti_(this,
                           "post_write",
                           "Notification immediately after the register has been written",
                           "post_write"),
          post_read_noti_(this,
                          "post_read",
                          "Notification immediately after the register has been read",
                          "post_read")
    {
        if(parent != nullptr){
            setExpectedParent_(parent);
        }

        sparta_assert(def, "Cannot construct a register with a null definition");
        sparta_assert(def->name != 0, "Cannot have a null name in a register definition");

        if(!strcmp(def->group, GROUP_NAME_NONE) && (def->group_num != GROUP_NUM_NONE)){
            throw SpartaException("Attempted to add register \"")
                << getLocation() << "\" which had group number " << def->group_num
                << " but had group name \"\". A group name is required if a group number is specified.";
        }

        if(strcmp(def->group, GROUP_NAME_NONE) && (def->group_num == GROUP_NUM_NONE)){
            throw SpartaException("Attempted to add register \"")
                << getLocation() << "\" which had group number GROUP_NUM_NONE"
                << " but had group name \"" << def->group
                << "\". A group number is required if a group name is specified.\"" << GROUP_NAME_NONE << "\"";
        }

        // Ensure byte-size is valid
        static_assert(std::is_unsigned<decltype(def->bytes)>::value == true);
        if(!isPowerOf2(def->bytes) || def->bytes == 0){
            throw SpartaException("Register \"")
                << getName() << "\" size in bytes must be a power of 2 larger than 0, is "
                << def->bytes;
        }

        // Add all fields
        for(auto & fdp : def->fields){
            addField(fdp);
        }

        // Add all aliases
        const char* const * ap = def->aliases;
        if(ap != 0){
            while(*ap != 0){
                addAlias(*ap); // From sparta::TreeNode
                ++ap;
            }
        }

        // Must add child only after assigning all aliases
        if(parent != nullptr){
            parent->addChild(this);
        }
    }

    /*!
     * \brief Destructor
     *
     * Deletes all fields allocated by this instance
     */
    virtual ~RegisterBase()
    {
        for(Field* f : owned_fields_){
            delete f;
        }
    }

    /*!
     * \brief Reset this register to its default value
     * \note Uses poke internally so no notifications are posted
     */
    void reset(bool unmasked=true) {
        // Poke the whole value, one-byte at a time
        for(size_type i=0; i<def_->bytes; i++){
            if(unmasked){
                pokeUnmasked(def_->initial_value[i], i);
            }else{
                poke(def_->initial_value[i], i);
            }
        }
    }

    /*!
     * \brief Create a new field in this Register based on the given
     * Field::Definition
     * \param fd Field Definition describing the new field to create.
     * fd must exist for the duration of this object since a reference will
     * be kept.
     * \pre Register TreeNode cannot be built
     *
     * Deliberately allows addition of fields through this interface in
     * addition to those in the Register::Definition with which this
     * Register was constructed.
     *
     * This method is also used internally to add fields from the
     * Register::Definition specified at construction.
     */
    Field* addField(const Field::Definition & fd) {
        sparta_assert(!isBuilt());
        owned_fields_.push_back(new Field(*this, fd));
        return owned_fields_.back();
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Const Attributes
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Gets the ID of this register as specified in its definition
     */
    ident_type getID() const { return def_->id; }

    /*!
     * \brief Gets the group number of this register as specified in its
     * definition
     */
    group_num_type getGroupNum() const { return def_->group_num; }

    /*!
     * \brief Gets the name of the group to which this register belongs
     * from its definition.
     * \see getGroupNum
     */
    std::string getGroupName() const {
        // Return group. Guaranteed not to be null by Register::Register
        return def_->group;
    }

    /*!
     * \brief Gets the group index of this register as specified in its
     * definition
     */
    group_idx_type getGroupIdx() const { return def_->group_idx; }

    /*!
     * \brief Gets the size of this register's value in bytes
     */
    size_type getNumBytes() const { return def_->bytes; }

    /*!
     * \brief Gets the size of this register's value in bits
     */
    size_type getNumBits() const { return bits_; }

    /*!
     * \brief Gets the number of fields within this register
     */
    size_type getNumFields() const { return (size_type)fields_.size(); }

    /*!
     * \brief Determines if this register is accessible through a specific
     * bank index (assuming that this bank index exists in the owning register
     * set)
     * \param bank Bank index
     * \return true if this register is accessible through its containing
     * register set in the specified bank. Alternatively, will return true if
     * this register is unbanked (isBanked() == false) regardless of the value
     * of \a bank. However, a real registerSet may only have N banks where N is
     * less than the value of \a bank for the purposes of this test. In this
     * situation, this register would not be available through that register set
     * through \a bank because \a bank is not available in that register set.
     * If that register set were to contain \a bank, then any unbanked register
     * would be available at that bank.
     */
    bool isInBank(bank_idx_type bank) const {
        if(isBanked() == false){
            return true;
        }

        auto itr = std::find(def_->bank_membership.begin(), def_->bank_membership.end(), bank);
        return itr != def_->bank_membership.end();
    }

    /*!
     * \brief Is this register banked.
     * \return true if banked (register had 1 or more elements in its
     * definition's bank membership vector). false if not unbanked (register
     * had 0 elements in its definition's bank membership vector)
     */
    bool isBanked() const {
        return def_->bank_membership.size() != 0;
    }

    /*!
     * \brief Gets the full set of register fields
     */
    FieldVector& getFields() { return fields_; }

    /*!
     * \brief Const qualified version of getFields
     */
    const FieldVector& getFields() const { return fields_; }

    /*!
     * \brief Returns the identity of the compound Register in the same
     * Register set which this register is a subset (refers a subset of
     * bytes in that parent).
     * \return ID of register of which this is a subset if any. If none,
     * returns INVALID_ID
     */
    ident_type getSubsetOf() const { return def_->subset_of; }

    /*!
     * \brief Returns the byte offset into the compound Register of which
     * this register is a subset
     * \see getSubsetOf
     * \return byte offset into the register of which this is a subset.
     * If this register is not a subset of another
     * (getSubsetOf() == INVALID_ID), returns
     * returns INVALID_ID
     */
    size_type getSubsetOffset() const { return def_->subset_offset; }

    /*!
     * \brief Returns the hint flags for this type.
     * \see HintFlag
     */
    Definition::HintsT getHintFlags() const { return def_->hints; }

    /*!
     * \brief Returns the regdomain for this type.
     * \see RegdomainFlag
     */
    Definition::RegDomainT getRegDomain() const { return def_->regdomain; }

    /*!
     * \brief Gets the definition supplied during the construciton of this
     * Register.
     * \return References to the same const Definition object supplied at
     * construction.
     */
    const Definition& getDefinition() const {
        return *def_;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Access Methods
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Read a value from this register
     */
    template <typename T>
    T read(index_type idx=0)
    {
        T tmp;
        read(&tmp, sizeof(tmp), idx * sizeof(tmp));
        return tmp;
    }

    /*!
     * \brief Read a value from this register, possibly via a user-supplied callback
     */
    sparta::utils::ValidValue<uint64_t> readWithCheck()
    {
        if (hasReadCB()) {
            return read_with_check_cb_(this);
        } else if (getNumBytes()==4) {
            return read<uint32_t>();
        }

        sparta_assert(getNumBytes()==8,
                    "read callback only support for 4- and 8-byte registers");
        return read<uint64_t>();
    }

    /*!
     * \brief Write a value into this register
     * \note Write-mask is applied
     */
    template <typename T>
    void write(T val, index_type idx=0)
    {
        write(&val, sizeof(val), idx * sizeof(val));
    }

    /*!
     * \brief Write a value to this register, possibly via a user-supplied callback
     */
    template <typename T>
    bool writeWithCheck(T val)
    {
        if (hasWriteCB()) {
            static_assert((sizeof(T)==4) || (sizeof(T)==8),
                          "write callback only support for 4- and 8-byte registers");
            return write_with_check_cb_(this, val);
        }
        write<T>(val);
        return true;
    }

    /*!
     * \brief Write a value into this register without it being affected by the
     * write-mask
     * \warning This ignores read-only fields
     */
    template <typename T>
    void writeUnmasked(T val, index_type idx=0)
    {
        writeUnmasked(&val, sizeof(val), idx * sizeof(val));
    }

    template <typename T>
    T peek(index_type idx=0) const
    {
        T tmp;
        peek(&tmp, sizeof(tmp), idx * sizeof(tmp));
        return tmp;
    }

    /*!
     * \brief Poke a value into this regster
     * \note Write-mask is applied.
     */
    template <typename T>
    void poke(T val, index_type idx=0)
    {
        poke(&val, sizeof(val), idx * sizeof(val));
    }

    /*!
     * \brief Poke a value into this regster without it being affected by the
     * write-mask
     * \warning This ignores read-only fields
     */
    template <typename T>
    void pokeUnmasked(T val, index_type idx=0)
    {
        pokeUnmasked(&val, sizeof(val), idx * sizeof(val));
    }

    /*!
     * \brief Read value directly from the Register's backing store
     */
    template <typename T>
    T dmiRead(index_type idx = 0) const
    {
        T res;
        dmiRead_(&res, sizeof(res), sizeof(res) * idx);
        return res;
    }

    /*!
     * \brief Write a value directly to this Register's backing store
     * \note No masking, boundary checkor or notification is performed
     */
    template <typename T>
    void dmiWrite(T val, index_type idx = 0)
    {
        dmiWrite_(&val, sizeof(val), sizeof(val) * idx);
    }

    /*!
     * \brief Get a write mask at the given index of the given size
     * \param idx Index of mask to access. Gets the mask associated with the
     * n'th \a T in the write mask
     * \throw SpartaException if idx and type \a T do not refer to a set of bytes
     * within the size of this register
     */
    template <typename T>
    T getWriteMask(index_type idx=0) const
    {
        sparta_assert((idx + 1) * sizeof(T) <= mask_.getSize());
        return *(reinterpret_cast<const T*>(mask_.getValue()) + idx);
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Printing Methods
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Creates a string representing this register's value as a
     * sequence of bytes, separated by a spaces, sorted by address ascending.
     *
     * \verbatim
     * (7) 00 03 00 ad 00 ff ff ba (0)
     * \endverbatim
     */
    std::string getValueAsByteString() const
    {
        const auto size = getNumBytes();
        std::vector<uint8_t> value(size);
        peek(value.data(), size, 0);
        return utils::bin_to_hexstr(value.data(), size);
    }

    /*!
     * \brief Creats a string representing this register's write-mask as
     * sequence of bytes, separated by spaces, sorted by address ascending.
     *
     * \verbatim
     * (7) 00 00 00 00 03 00 00 00 (0)
     * \endverbatim
     */
    std::string getWriteMaskAsByteString() const
    {
        return utils::bin_to_hexstr(
            reinterpret_cast<const uint8_t *>(mask_.getValue()), mask_.getSize());
    }

    /*!
     * \brief Creats a string representing this register's write-mask as
     * sequence of bits, separated by a space between each byte, sorted by
     * bit-address ascending
     *
     * \verbatim
     * 00001000 00000011
     * \endverbatim
     */
    std::string getWriteMaskAsBitString() const
    {
        return utils::bin_to_bitstr(
            reinterpret_cast<const uint8_t *>(mask_.getValue()), mask_.getSize());
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Retrieves a child that is a Register::Field with the given
     * dotted path
     * \note no pattern matching supported in this method
     * \note Generally, only immediate children can be fields.
     * \throw SpartaException if child which is a Register::Field is not found
     */
    Field *getField(const std::string &name)
    {
        return getChildAs<Field *>(name);
    }

    /*!
     * \brief Retrieves a child that is a Register::Field with the given
     * dotted path
     * \note no pattern matching supported in this method
     * \note Generally, only immediate children can be fields.
     * \throw SpartaException if child which is a Register::Field is not found
     */
    const Field *getField(const std::string &name) const
    {
        return getChildAs<Field *>(name);
    }

    //! \name Observation
    //! @{
    ////////////////////////////////////////////////////////////////////////

    void read(void *buf, size_t size, size_t offset)
    {
        sparta_assert(offset + size <= getNumBytes(), "Access out of bounds");
        read_(buf, size, offset);
    }

    void peek(void *buf, size_t size, size_t offset) const
    {
        sparta_assert(offset + size <= getNumBytes(), "Access out of bounds");
        peek_(buf, size, offset);
    }

    void write(const void *buf, size_t size, size_t offset)
    {
        sparta_assert(offset + size <= getNumBytes(), "Access out of bounds");

        BitArray old = peekBitArray_(size, offset);
        BitArray val(reinterpret_cast<const uint8_t *>(buf), size);
        BitArray mask = mask_ >> 8 * offset;

        old = (old & ~mask) | (val & mask);
        write_(old.getValue(), size, offset);
    }

    void writeUnmasked(const void *buf, size_t size, size_t offset)
    {
        sparta_assert(offset + size <= getNumBytes(), "Access out of bounds");
        write_(buf, size, offset);
    }

    void poke(const void *buf, size_t size, size_t offset)
    {
        sparta_assert(offset + size <= getNumBytes(), "Access out of bounds");

        BitArray old = peekBitArray_(size, offset);
        BitArray val(reinterpret_cast<const uint8_t *>(buf), size);
        BitArray mask = mask_ >> 8 * offset;

        old = (old & ~mask) | (val & mask);
        poke_(old.getValue(), size, offset);
    }

    void pokeUnmasked(const void *buf, size_t size, size_t offset)
    {
        sparta_assert(offset + size <= getNumBytes(), "Access out of bounds");
        poke_(buf, size, offset);
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Returns the post-write notification-source node for this
     * register which can be used to observe writes to this register.
     * This notification is posted immediately after the register has
     * been written and populates a sparta::Register::PostWriteAccess object
     * with the results.
     *
     * Refer to PostWriteNotiSrc (sparta::NotificationSource) for more
     * details. Use the sparta::NotificationSource::registerForThis and
     * sparta::NotificationSource::deregisterForThis methods.
     */
    PostWriteNotiSrc &getPostWriteNotificationSource()
    {
        return post_write_noti_;
    }

    /*!
     * \brief Returns the read notification-source node for this
     * register which can be used to observe reads to this register.
     * This notification is posted immediately after the register has
     * been read and populates a sparta::Register::ReadAccess object
     * with the results.
     *
     * Refer to ReadNotiSrc (sparta::NotificationSource) for more
     * details. Use the sparta::NotificationSource::registerForThis and
     * sparta::NotificationSource::deregisterForThis methods.
     */
    ReadNotiSrc &getReadNotificationSource()
    {
        return post_read_noti_;
    }

    /*!
     * \brief Registers a callback for obtaining a 32- or 64-bit register value
     */
    void addReadCB(register_read_callback_type callback)
    {
        read_with_check_cb_ = callback;
    }

    /*!
     * \brief Returns true if register has a read callback; false otherwise
     */
    bool hasReadCB() const
    {
        return read_with_check_cb_ != nullptr;
    }

    /*!
     * \brief Registers a callback for writing a 32- or 64-bit register value
     */
    void addWriteCB(register_write_callback_type callback)
    {
        write_with_check_cb_ = callback;
    }

    /*!
     * \brief Returns true if register has a write callback; false otherwise
     */
    bool hasWriteCB() const
    {
        return write_with_check_cb_ != nullptr;
    }

protected:

    /*!
     * \brief React to child registration
     * \param child TreeNode child. Typically, this should be downcastable
     * to a sparta::Register::Field. This is a borrowed reference - child is
     * *not* copied. Child lifetime must exceed that of this Register
     * instance.
     *
     * Overrides TreeNode::onAddingChild_
     */
    virtual void onAddingChild_(TreeNode* child) override {
        Field* fld = dynamic_cast<Field*>(child);
        if(nullptr != fld){
            // Add Field to fields_ list for tracking.
            fields_.push_back(fld);
        }
    }

    virtual void read_(void *buf, size_t size, size_t offset = 0) = 0;

    virtual void peek_(void *buf, size_t size, size_t offset = 0) const = 0;

    virtual void write_(const void *buf, size_t size, size_t offset = 0) = 0;

    virtual void poke_(const void *buf, size_t size, size_t offset = 0) = 0;

    virtual void dmiRead_(void *buf, size_t size, size_t offset = 0) const
    {
        (void)buf; (void)size; (void)offset;
        sparta_assert(!"Register DMI not supported");
    }

    virtual void dmiWrite_(const void *buf, size_t size, size_t offset = 0)
    {
        (void)buf; (void)size; (void)offset;
        sparta_assert(!"Register DMI not supported");
    }

private:
    BitArray computeWriteMask_(const Definition *def) const
    {
        const auto mask_size = def->bytes;
        BitArray write_mask(0, mask_size);
        BitArray partial_mask(0, mask_size);
        partial_mask.fill<uint8_t>(0xff);

        for (auto &fdp : def->fields) {
            if (fdp.read_only) {
                const uint64_t field_size = fdp.high_bit - fdp.low_bit + 1;
                const uint64_t shift_down = 8 * mask_size - field_size;
                const uint64_t shift_up = fdp.low_bit;

                write_mask |= ((partial_mask >> shift_down) << shift_up);
            }
        }

        return ~write_mask;
    }

    BitArray peekBitArray_(size_t size, size_t offset=0) const
    {
        std::vector<uint8_t> value(size);
        peek_(value.data(), size, offset);
        return BitArray(value.data(), size);
    }

    /*!
     * \brief Register definition given at construction (not copied)
     */
    const Definition* def_;

    /*!
     * \brief All Fields allocated by this set. These fields are deleted
     * at destruction of this Register
     */
    FieldVector owned_fields_;

    /*!
     * \brief All Fields that are available in this register
     */
    FieldVector fields_;

    /*!
     * \brief Width of this register in bits
     */
    const size_type bits_;

    /*!
     * \brief Bit mask with zeros in the bit positions that are read-only
     */
    const BitArray mask_;

    /*!
     * \brief NotificationSource for post-write notifications
     */
    PostWriteNotiSrc post_write_noti_;

    /*!
     * \brief NotificationSource for post-read notifications
     */
    ReadNotiSrc post_read_noti_;

    /*!
     * \brief Callbacks for readWithCheck and writeWithCheck
     */
    register_read_callback_type read_with_check_cb_ {nullptr};
    register_write_callback_type write_with_check_cb_ {nullptr};
}; // class RegisterBase

inline bool operator==(const RegisterBase::Definition &a,
                       const RegisterBase::Definition &b)
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
    if (a.bytes != b.bytes) {
        return false;
    }

    /* TODO: Compare fields. This require a == operator for Field::Definition */

    if (a.bank_membership != b.bank_membership) {
        return false;
    }

    auto a_aliases = a.aliases;
    auto b_aliases = b.aliases;
    if (a_aliases != nullptr && b_aliases != nullptr) {
        for (; *a_aliases != 0 || *b_aliases != 0; ++a_aliases, ++b_aliases) {
            if (!utils::strcmp_with_null(*a_aliases, *b_aliases)) {
                return false;
            }
        }
    } else if (!(a_aliases == nullptr && b_aliases == nullptr)) {
        return false;
    }

    if (a.subset_of != b.subset_of) {
        return false;
    }
    if (!utils::strcmp_with_null((const char *)a.initial_value, (const char *)b.initial_value)) {
        return false;
    }
    if (a.hints != b.hints) {
        return false;
    }
    if (a.regdomain != b.regdomain) {
        return false;
    }

    return true;
}

inline bool operator!=(const RegisterBase::Definition &a,
                       const RegisterBase::Definition &b)
{
    return !(a == b);
}

class Register : public RegisterBase
{
public:
    Register(TreeNode *parent, const Definition *def, ArchData *adata) :
        RegisterBase(parent, def),
        dview_(adata, def->id, def->bytes, def->subset_of, def->subset_offset, def->initial_value),
        prior_val_dview_(adata, DataView::INVALID_ID, def->bytes),
        post_write_noti_data_(this, &prior_val_dview_, &dview_),
        post_read_noti_data_(this, &dview_)
    {
    }

    // Override from TreeNode
    std::string stringize(bool pretty = false) const override
    {
        (void)pretty;
        std::stringstream ss;
        ss << '<' << getLocation() << " " << getNumBits() << " bits ";
        if (dview_.isPlaced()) {
            ss << getValueAsByteString();
        } else {
            ss << DataView::DATAVIEW_UNPLACED_STR;
        }
        ss << '>';
        return ss.str();
    }

private:
    /*!
     * \brief Discover and store the raw location of this Register's data
     */
    void onBindTreeEarly_() override
    {
        /* Arch data must laid out before we can get pointer into it */
        sparta_assert(dview_.getArchData()->isLaidOut());
        raw_data_ptr_ = dview_.getLine()->getRawDataPtr(dview_.getOffset());
    }

    void read_(void *buf, size_t size, size_t offset=0) override final
    {
        auto &post_read_noti = getReadNotificationSource();

        peek_(buf, size, offset);
        if (__builtin_expect(post_read_noti.observed(), false)) {
            post_read_noti.postNotification(post_read_noti_data_);
        }
    }

    void peek_(void *buf, size_t size, size_t offset=0) const override final
    {
        dview_.getLine()->read(
            dview_.getOffset() + offset, size, static_cast<uint8_t *>(buf));
    }

    void dmiRead_(void *buf, size_t size, size_t offset = 0) const override final
    {
        memcpy(buf, raw_data_ptr_ + offset, size);
    }

    void write_(const void *buf, size_t size, size_t offset=0) override final
    {
        auto &post_write_noti = getPostWriteNotificationSource();

        if (__builtin_expect(post_write_noti.observed(), false)) {
            prior_val_dview_ = dview_;
            poke_(buf, size, offset);
            post_write_noti.postNotification(post_write_noti_data_);
        } else {
            poke_(buf, size, offset);
        }
    }

    void poke_(const void *buf, size_t size, size_t offset=0) override final
    {
        dview_.getLine()->write(
            dview_.getOffset() + offset, size, static_cast<const uint8_t *>(buf));
    }

    void dmiWrite_(const void *buf, size_t size, size_t offset = 0) override final
    {
        memcpy(raw_data_ptr_ + offset, buf, size);
        dview_.getLine()->flagDirty();
    }

    /*!
     * \brief DataView representing this Register's bytes in some ArchData
     */
    DataView dview_;

    /*!
     * \brief DataView used to store the prior value of this register when
     * needed (e.g. post-write notifications)
     */
    DataView prior_val_dview_;

    /*!
     * \brief Data associated with a post-write notification
     */
    PostWriteNotiSrc::data_type post_write_noti_data_;

    /*!
     * \brief Data associated with a post-read notification
     */
    ReadNotiSrc::data_type post_read_noti_data_;

    /*!
     * \brief Pointer to run-time storage location of this Register's data
     */
    uint8_t *raw_data_ptr_ = nullptr;
};

} // namespace sparta


//! ostream insertion operator for sparta::Register
inline std::ostream& operator<<(std::ostream& o, const sparta::Register& reg){
    o << reg.stringize();
    return o;
}

//! ostream insertion operator for sparta::Register
inline std::ostream& operator<<(std::ostream& o, const sparta::Register* reg){
    if(reg == 0){
        o << "null";
    }else{
        o << reg->stringize();
    }
    return o;
}


//! ostream insertion operator for sparta::Register::Field
inline std::ostream& operator<<(std::ostream& o, const sparta::Register::Field& field){
    o << field.stringize();
    return o;
}

//! ostream insertion operator for sparta::Register::Field
inline std::ostream& operator<<(std::ostream& o, const sparta::Register::Field* field){
    if(field == 0){
        o << "null";
    }else{
        o << field->stringize();
    }
    return o;
}


//! \brief Required in simulator source to define some globals.
#define SPARTA_REGISTER_BODY                                              \
    constexpr sparta::RegisterBase::group_num_type sparta::RegisterBase::GROUP_NUM_NONE; \
    const sparta::RegisterBase::Definition sparta::RegisterBase::DEFINITION_END{0, nullptr, 0, nullptr, 0, nullptr, 0, { }, { }, nullptr, 0, 0, 0, 0, 0};


// __REGISTER_H__
#endif
