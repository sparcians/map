

#ifndef _H_SPARTA_TAG
#define _H_SPARTA_TAG

#include <inttypes.h>
#include <string>
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{

/**
 * \brief Tag(): Simple class to provide nested sequence
 *        numbering
 *
 * Tags are identifiers for objects in simulation.  Tags are nested,
 * meaning that one Tag can be a child of another.  In this case, the
 * the tag is labeled like so: <parent>.<child>[.<child>]*
 */
class Tag
{
    typedef uint32_t SequenceType;
public:
    /**
     * \brief Tag(): Constructor
     *  Construct a new tag, using the internal global sequence number
     */
    Tag() :
        this_seq_ (global_seq_++)
    {}

    /**
     * \brief Tag(): Copy constructor
     *
     * \param other instance to copy
     */
    Tag(const Tag& other) = default;

    /**
     * \brief Tag(): Construct as a child of a parent Tag. This will
     *        increment the child sequence counter in the parent
     *
     * \param parent Parent tag -- can be nullptr meaning that is
     * really doesn't have a parent
     */
    Tag(Tag* parent) :
        parent_(parent)
    {
        if(SPARTA_EXPECT_TRUE(parent != nullptr)) {
            this_seq_ = parent->child_seq_;
            ++(parent->child_seq_);
        }
        else {
            this_seq_ = global_seq_++;
        }
    }

    /**
     * \brief std::string(): String casting operator
     *
     * \return std::string Full tag string "parent.child" format
     */
    operator std::string() const
    {
        std::stringstream ss;
        if(parent_) {
            ss << std::string(*parent_) << '.' << this_seq_;
        }
        else {
            ss << this_seq_;
        }
        return ss.str();
    }

    /**
     * \brief ==(): Equality operator
     *
     * \param other Other tag to compare
     *
     * \return bool TRUE if same
     */
    bool operator==(const Tag& other) const
    {
        if(parent_) {
            if(other.parent_ == nullptr) {
                // not equivalent, not the same hierarchy
                return false;
            }
            return (this_seq_ == other.this_seq_) &&
                (*parent_ == *other.parent_);
        }
        else {
            if(other.parent_ != nullptr) {
                // not equivalent, not the same hierarchy
                return false;
            }
            return (this_seq_ == other.this_seq_);
        }
    }

    /**
     * \brief !=(): Inequality operator
     *
     * \param other Other tag to compare
     *
     * \return bool TRUE if different
     */
    bool operator!=(const Tag& other) const
    {
        return !operator==(other);
    }

    /**
     * \brief ==(): Equality operator (vs. string)
     *
     * \param s String to compare
     *
     * \return bool TRUE if same
     */
    bool operator==(const std::string& s) const
    {
        return (std::string(*this) == s);
    }

    /**
     * \brief !=(): Inequality operator (vs. string)
     *
     * \param s String to compare
     *
     * \return bool TRUE if different
     */
    bool operator!=(const std::string& s) const
    {
        return !operator==(s);
    }

    static void resetGlobalSeqNum() {
        global_seq_ = 1;
    }

private:
    Tag                    *parent_     = nullptr; //!< This Tag's parent
    SequenceType            this_seq_  = 0; //!< Our sequence number
    SequenceType            child_seq_ = 1; //!< Next child sequence number to assign
    static SequenceType     global_seq_;    //!< Global sequence number
};

/**
 * \brief <<(): Stream insertion operator
 *
 * \param os Output stream
 * \param tag Tag to output
 *
 * \return std::ostream&
 */
inline std::ostream& operator<<(std::ostream& os, const Tag& tag)
{
    os << std::string(tag);
    return os;
}

/**
 * \brief <<(): Stream insertion operator (for Tag pointers)
 *
 * \param os Output stream
 * \param tag Tag pointer to de-reference and output
 *
 * \return std::ostream&
 */
inline std::ostream& operator<<(std::ostream& os, const Tag* tag)
{
    if (tag != nullptr) {
        os << std::string(*tag);
    } else {
        // throw an exception here?
        os << "NULL TAG";
    }

    return os;
}


} // sparta

#define SPARTA_TAG_BODY                           \
    namespace sparta {                            \
        Tag::SequenceType Tag::global_seq_ = 1; \
    }

#endif //_H_SPARTA_TAG
