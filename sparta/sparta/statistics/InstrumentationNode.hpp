// <InstrumentationNode> -*- C++ -*-

/*!
 * \file InstrumentationNode.hpp
 * \brief Virtual interface node for simulator instrumentation (e.g. counters,
 * stats, nontifications).
 */

#ifndef __INSTRUMENTATION_NODE__
#define __INSTRUMENTATION_NODE__

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/Utils.hpp"

namespace sparta {

class StatisticInstance;

/*!
 * \brief Base class requiring
 */
class InstrumentationNode : public TreeNode
{
public:

    //! \name Types
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Continuous visibility level. Several key points along
     * continum are indicated within Visibility
     */
    typedef uint32_t visibility_t;

    /*!
     * \brief Common visibility levels.
     * Visibility is a continum (visibility can be anywhere in [0 to
     * MAX_VISIBILITY])
     * \see getVisibility
     */
    enum Visibility {
        /*!
         * \brief Hidden hint. Lowest possible visibility
         */
        VIS_HIDDEN  = 0,

        /*!
         * \brief Supporting data. Used mainly for counters which contain an
         * intermediate value for the sole purpose of defining some StatisticDef
         */
        VIS_SUPPORT = 1000000, // 1M

        /*!
         * \brief Detailed data. Might be confusing or esoteric for
         * end-users
         */
        VIS_DETAIL  = 10000000, // 10M

        /*!
         * \brief Normal visibility (default)
         */
        VIS_NORMAL  = 100000000, // 100M

        /*!
         * \brief The next two visibility levels are for High-importance data.
         * These are split up in the (100M, 1B]-range to distinguish between
         * various levels of importance.
         */

        /*!
         * \brief 1) Important data, mostly useful for end-users familiar
         * with the unit where these statistics where defined (e.g., block-owners).
         */
        VIS_SUMMARY = VIS_NORMAL  * 2, // 100M * 2

        /*
         * If needed, more levels of visibility can be inserted here: VIS_SUMMARY_L2 - VIS_SUMMARY_L9
         */

        /*!
         * \brief 2) High-importance data that end-users should always see.
         * \note No Visibility levels should be higher than this because it
         * could overflow visibility_t if an average were taken. For example:
         * (VIS_CRITICAL + VIS_NORMAL / 2) is OK, but larger visibilities might
         * overflow
         */
        VIS_CRITICAL = 1000000000, // 1B

        /*!
         * \brief Maximum possible visibility
         */
        VIS_MAX     = ~(visibility_t)0

    };

    /*!
     * \brief The default sparta resource visibility value that
     * should be used. This is an alias of VIS_MAX at the moment.
     */
    static constexpr visibility_t AUTO_VISIBILITY = VIS_MAX;

    /*!
     * \brief the actual visibility that the sparta containers such
     * as buffer, queue, and array will use when VIS_SPARTA_DEFAULT is set.
     */
    static constexpr visibility_t CONTAINER_DEFAULT_VISIBILITY = VIS_HIDDEN;

    /*!
     * \brief Default node visibility
     */
    static constexpr visibility_t DEFAULT_VISIBILITY = (visibility_t)VIS_NORMAL;

    /*!
     * \brief Continuous Class level. Several key points along
     * continum are indicated within Class
     */
    typedef uint32_t class_t;

    /*!
     * \brief Common Class levels.
     * Class is a continum (class can be anywhere in [0 to
     * MAX_CLASS])
     * \see getClass
     */
    enum Class {
        /*!
         * \brief Other hint. Lowest possible class
         */
        CLASS_OTHER  = 0,

        /*!
         * \brief important class (default)
         */
        CLASS_IMPORTANT  = 50,

        /*!
         * \brief important class (default)
         */
        CLASS_CRITICAL  = 100,

        CLASS_MAX     = ~(class_t)0

    };

    /*!
     * \brief The default sparta resource class value that
     * should be used. This is an alias of CLASS_IMPORTANT at the moment.
     */
    static constexpr class_t AUTO_CLASS = CLASS_IMPORTANT;

    /*!
     * \brief the actual class that the sparta containers such
     * as buffer, queue, and array will use when CLASS_SPARTA_DEFAULT is set.
     */
    static constexpr class_t CONTAINER_DEFAULT_CLASS = CLASS_IMPORTANT;

    /*!
     * \brief Default node class
     */
    static constexpr class_t DEFAULT_CLASS = (class_t)CLASS_IMPORTANT;

    /*!
     * \brief Instrumentation types. All subclasses will provide this type
     * \see getInstrumentationType
     */
    enum Type {
        /*!
         * \brief Statisitic definition
         */
        TYPE_STATISTICDEF = 0,

        /*!
         * \brief Counter (of any subclass)
         */
        TYPE_COUNTER,

        /*!
         * \brief Parameter
         */
        TYPE_PARAMETER,

        /*!
         * \brief Histogram Node, which should have relevant stats and counters
         * as (indirect) children
         */
        TYPE_HISTOGRAM,

        /*!
         * \brief Maximum Type value (illegal)
         */
        NUM_TYPES

    };

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Construction
    //! @{
    ////////////////////////////////////////////////////////////////////////

    //! \brief Not default-constructable
    InstrumentationNode() = delete;

    /*!
     * \brief Not copy-constructable
     */
    InstrumentationNode(const InstrumentationNode&) = delete;

    /*!
     * \brief Move constructor
     * \pre rhp must not be fully finalized
     * \pre rhp must not have any observers registered directly on it
     * \pre Avoid move-constructing from InstrumentationNodes with children as
     * the children may fail to be re-added to this new InstrumentationNode
     * if they attempt to dynamic_cast this InstrumentationNode's pointer to a subclass
     * of InstrumentationNode.
     */
    InstrumentationNode(InstrumentationNode&& rhp) :
        TreeNode::TreeNode(std::move(rhp)),
        visibility_(rhp.visibility_),
        class_(rhp.class_),
        instrument_type_(rhp.instrument_type_)
    {
    }

    //! \brief Not assign-constructable
    InstrumentationNode& operator=(const InstrumentationNode&) = delete;

    /*!
     * \brief InstrumentationNode full constructor
     */
    InstrumentationNode(TreeNode* parent,
                        const std::string& name,
                        const std::string& group,
                        group_idx_type group_idx,
                        const std::string& desc,
                        Type type,
                        visibility_t visibility,
                        class_t n_class) :
        TreeNode::TreeNode(parent, name, group, group_idx, desc),
        visibility_(visibility),
        class_(n_class),
        instrument_type_(type)
    { }

    /*!
     * \brief InstrumentationNode full constructor
     */
    InstrumentationNode(TreeNode* parent,
                        const std::string& name,
                        const std::string& group,
                        group_idx_type group_idx,
                        const std::string& desc,
                        Type type,
                        visibility_t visibility) :
        TreeNode::TreeNode(parent, name, group, group_idx, desc),
        visibility_(visibility),
        class_(DEFAULT_CLASS),
        instrument_type_(type)
    { }

    /*!
     * \brief InstrumentationNode full constructor
     */
    InstrumentationNode(TreeNode* parent,
                        const std::string& name,
                        const std::string& group,
                        group_idx_type group_idx,
                        const std::string& desc,
                        Type type) :
        InstrumentationNode(parent,
                            name,
                            group,
                            group_idx,
                            desc,
                            type,
                            DEFAULT_VISIBILITY,
                            DEFAULT_CLASS)
    {
        // Delegated constructor
    }

    /*!
     * \brief InstrumentationNode constructor with no parent Node
     * \see other sparta::InstrumentationNode constructors
     */
    InstrumentationNode(const std::string& name,
                        const std::string& group,
                        group_idx_type group_idx,
                        const std::string& desc,
                        Type type) :
        InstrumentationNode(nullptr,
                            name,
                            group,
                            group_idx,
                            desc,
                            type,
                            DEFAULT_VISIBILITY,
                            DEFAULT_CLASS)
    {
        // Delegated constructor
    }

    /*!
     * \brief InstrumentationNode constructor with no parent Node
     * \see other sparta::InstrumentationNode constructors
     */
    InstrumentationNode(const std::string& name,
                        const std::string& group,
                        group_idx_type group_idx,
                        const std::string& desc,
                        Type type,
                        visibility_t visibility,
                        class_t n_class) :
        InstrumentationNode(nullptr,
                            name,
                            group,
                            group_idx,
                            desc,
                            type,
                            visibility,
                            n_class)
    {
        // Delegated constructor
    }

    /*!
     * \brief InstrumentationNode constructor with no parent Node
     * \see other sparta::InstrumentationNode constructors
     */
    InstrumentationNode(const std::string& name,
                        const std::string& group,
                        group_idx_type group_idx,
                        const std::string& desc,
                        Type type,
                        visibility_t visibility):
        InstrumentationNode(nullptr,
                            name,
                            group,
                            group_idx,
                            desc,
                            type,
                            visibility,
                            DEFAULT_CLASS)
    {
        // Delegated constructor
    }

    /*!
     * \brief InstrumentationNode constructor with no group information
     */
    InstrumentationNode(TreeNode* parent,
                        const std::string& name,
                        const std::string& desc,
                        Type type) :
        InstrumentationNode::InstrumentationNode(parent,
                                                 name,
                                                 GROUP_NAME_NONE,
                                                 GROUP_IDX_NONE,
                                                 desc,
                                                 type,
                                                 DEFAULT_VISIBILITY,
                                                 DEFAULT_CLASS)
    {
        // Delegated constructor
    }

    /*!
     * \brief InstrumentationNode constructor with no group information
     */
    InstrumentationNode(TreeNode* parent,
                        const std::string& name,
                        const std::string& desc,
                        Type type,
                        visibility_t visibility,
                        class_t n_class) :
        InstrumentationNode::InstrumentationNode(parent,
                                                 name,
                                                 GROUP_NAME_NONE,
                                                 GROUP_IDX_NONE,
                                                 desc,
                                                 type,
                                                 visibility,
                                                 n_class)
    {
        // Delegated constructor
    }

    /*!
     * \brief InstrumentationNode constructor with no group information
     */
    InstrumentationNode(TreeNode* parent,
                        const std::string& name,
                        const std::string& desc,
                        Type type,
                        visibility_t visibility):
        InstrumentationNode::InstrumentationNode(parent,
                                                 name,
                                                 GROUP_NAME_NONE,
                                                 GROUP_IDX_NONE,
                                                 desc,
                                                 type,
                                                 visibility,
                                                 DEFAULT_CLASS)
    {
        // Delegated constructor
    }


    /*!
     * \brief InstrumentationNode constructor with no parent node or
     * information
     */
    InstrumentationNode(const std::string& name,
                        const std::string& desc,
                        Type type) :
        InstrumentationNode::InstrumentationNode(nullptr,
                                                 name,
                                                 GROUP_NAME_NONE,
                                                 GROUP_IDX_NONE,
                                                 desc,
                                                 type,
                                                 DEFAULT_VISIBILITY,
                                                 DEFAULT_CLASS)
    {
        // Delegated constructor
    }

    /*!
     * \brief InstrumentationNode constructor with no parent node or
     * information
     */
    InstrumentationNode(const std::string& name,
                        const std::string& desc,
                        Type type,
                        visibility_t visibility,
                        class_t n_class) :
        InstrumentationNode::InstrumentationNode(nullptr,
                                                 name,
                                                 GROUP_NAME_NONE,
                                                 GROUP_IDX_NONE,
                                                 desc,
                                                 type,
                                                 visibility,
                                                 n_class)
    {
        // Delegated constructor
    }

    /*!
     * \brief InstrumentationNode constructor with no parent node or
     * information
     */
    InstrumentationNode(const std::string& name,
                        const std::string& desc,
                        Type type,
                        visibility_t visibility):
        InstrumentationNode::InstrumentationNode(nullptr,
                                                 name,
                                                 GROUP_NAME_NONE,
                                                 GROUP_IDX_NONE,
                                                 desc,
                                                 type,
                                                 visibility,
                                                 DEFAULT_CLASS)
    {
        // Delegated constructor
    }

    /*!
     * \brief Virtual destructor
     */
    virtual ~InstrumentationNode()
    {
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Attributes
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Gets the visibility hint of this node. This is invariant after
     * construction
     */
    visibility_t getVisibility() const {
        return visibility_;
    }

    /*!
     * \brief Gets the class hint of this node. This is invariant after
     * construction
     */
    class_t getClass() const {
        return class_;
    }

    /*!
     * \brief Gets the instrumentaiton type hint of this node. This is invariant
     * after construction
     */
    Type getInstrumentationType() const {
        return instrument_type_;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Printing Methods
    //! @{
    ////////////////////////////////////////////////////////////////////////

    virtual bool groupedPrinting(const std::vector<const StatisticInstance*> & sub_stats,
                                 std::set<const void*> & dont_print_these,
                                 void * grouped_json,
                                 void * doc) const {
        (void) sub_stats;
        (void) dont_print_these;
        (void) grouped_json;
        (void) doc;
        return false;
    }

    virtual bool groupedPrintingReduced(const std::vector<const StatisticInstance*> & sub_stats,
                                        std::set<const void*> & dont_print_these,
                                        void * grouped_json,
                                        void * doc) const {
        (void) sub_stats;
        (void) dont_print_these;
        (void) grouped_json;
        (void) doc;
        return false;
    }

    virtual bool groupedPrintingDetail(const std::vector<const StatisticInstance*> & sub_stats,
                                       std::set<const void*> & dont_print_these,
                                       void * grouped_json,
                                       void * doc) const {
        (void) sub_stats;
        (void) dont_print_these;
        (void) grouped_json;
        (void) doc;
        return false;
    }

    using StringPair = std::pair<std::string, std::string>;
    const std::vector<StringPair> & getMetadata() const {
        return metadata_;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

protected:

    /*!
     * \brief Add any arbitrary metadata as strings to this object. Used to
     * add extra information to statistics reports (json, etc.)
     */
    void addMetadata_(const std::string & key, const std::string & value){
        metadata_.emplace_back(std::make_pair(key, value));
    }

private:

    /*!
     * \brief Visibility hint for this node
     */
    visibility_t visibility_;

    /*!
     * \brief Class hint for this node
     */
    class_t class_;

    /*!
     * \brief Type hint for this node
     */
    Type instrument_type_;

    /*!
     * \brief
     */
    std::vector<StringPair> metadata_;
};

} // namespace sparta

// __INSTRUMENTATION_NODE__
#endif
