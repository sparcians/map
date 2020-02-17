// <Resource.hpp> -*- C++ -*-

/*!
 * \file   Resource.hpp
 *
 * \brief  File that defines the Resource class. Consider using sparta::Unit instead
 */

#ifndef __RESOURCE__H__
#define __RESOURCE__H__

#include <string>
#include <ostream>
#include <type_traits>

#include "sparta/simulation/ResourceContainer.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Utils.hpp"
#include "simdb/Errors.hpp"

namespace sparta {

    class TreeNode;

    /*!
     * \brief Information describing the type of validation being done.
     */
    class PostRunValidationInfo
    {
    public:
        // Empty for now
    };


    /*!
     * \class Resource
     * \brief The is the base class for all types of resources used by the SPARTA
     * framework
     */
    class Resource
    {
        //! Allow TreeNode access to onStartingTeardown_
        friend class TreeNode;

    public:

        /*!
         * \brief Construct resource with a resource container
         * \param rc ResourceContainter that will hold this Resource until the
         * Resource is destructed. Name and clock are extracted from this
         * container. Must not be nullptr
         */
        Resource(TreeNode* rc);

        /*!
         * \brief Construct resource with a specific name and resource container
         * \note It is recommended to use another consturctor with the name
         * extracted from the ResourceContainer instead of specifying a name.
         * \param rc ResourceContainter that will hold this Resource until the
         * Resource is destructed. Clock is extracted from this container. Must
         * not be nullptr
         * \param name Name to give the resource
         */
        Resource(TreeNode* rc,
                 const std::string& name);

        /*!
         * \brief Construct a Resource with the given name and clock having NO
         * association with a resource container. This constructor is reserved
         * for free-standing resources owned by other resources (not containers
         * [TreeNodes])
         * \param clk The clock this Resource belongs to
         * \param name The name of this Resource
         * \note This constructor is deprecated
         */
        Resource(const std::string & name,
                 const Clock * clk);

        /// Destroy!
        virtual ~Resource();

        /*!
         * \return Pointer to this resource's clock
         */
        const Clock * getClock() const;

        /*!
         * \return Pointer to this resource's scheduler
         */
        Scheduler * getScheduler(const bool must_exist = true) const;

        /*!
         * \return This Resource's name
         */
        std::string getName() const;

        /*!
         * \brief Gets the TreeNode (container) for this resource (if any)
         * \note TreeNode is a subclass of ResourcecContainer
         */
        TreeNode* getContainer();

        /*!
         * \brief Gets the TreeNode (container) for this resource (if any)
         * \note TreeNode is a subclass of ResourcecContainer
         */
        const TreeNode* getContainer() const;

        /*!
         * \brief Gets the ResourceContainer for this resource (if any)
         */
        ResourceContainer* getResourceContainer();

        /*!
         * \brief Gets the ResourceContainer for this resource (if any)
         */
        const ResourceContainer* getResourceContainer() const;

        /*!
         * Lets the resource know when its container is linked with another TreeNode
         *
         * \param node The TreeNode the container got linked with
         * \param label The label of the TreeNode the container got linked with
         */
        virtual void addLink(TreeNode *node, const std::string &label)
        {
            (void)node; (void)label;
            /* Resource that do not overload this method does not use the links */
        }

        /*!
         *  Lets the resource know that one of its link are now active
         *
         * \param label The label of the TreeNode the container got linked with
         */
        virtual void activateLink(const std::string &label)
        {
            (void)label;
            /* Resource that do not overload this method does not use the links */
        }

        //! Disallow copying
        Resource(const Resource &) = delete;
        Resource &operator=(const Resource &) = delete;

    private:

        /*!
         * \brief Called after simulation is done, but before statistic/report collection/generation
         * \note This can be invoked multiple times during a simulation with
         * different info content
         * \throw This method should throw an exception if any invalid state is
         * found
         * \see TreeNode::validatePostRun_
         */
        virtual void simulationTerminating_();

        /*!
         * \brief Alerts this resource that running has ended and this resource
         * should check its state for sanity. Resource may throw an exception
         * if invalid state is detected
         * \param info Information about the validation. This could be used to
         * enable/disable certain validation depending on what is requested
         * \note This is enabled/disbled by the simulator, so it may not always
         * be invoked.
         * \note This can be invoked multiple times during a simulation with
         * different info content
         * \throw This method should throw an exception if any invalid state is
         * found
         * \see TreeNode::validatePostRun_
         */
        virtual void validatePostRun_(const PostRunValidationInfo& info) const;

        /*!
         * \brief Alerts this resource that the simulation is exiting with an
         * error (and debug-dumping is enabled). It may be possible for the
         * simulator to call this earlier, however.
         * \note This is enabled/disbled by the simulator, so it may not always
         * be invoked.
         * \param output stream to which output data should be written. This
         * is expected to be a new ostream and will be discarded after use.
         * This method is not required to clean up the \a output ostream
         * \throw Should never throw
         * \note Conventionally, this method should write to \a output only.
         * Writing entirely new files from this method is unexpected by the user
         * and violates one of the design principals of this library where
         * the only simulator input and output files and named explicitly by the
         * user. If you must create a new file, please refer to it clearly from
         * within the \a output stream
         * \note Data can also be written to logs during this method as well,
         * but having all state in the debug dump simplifies debugging
         * \see TreeNode::validatePostRun_
         */
        virtual void dumpDebugContent_(std::ostream& output) const;

        /*!
         * \brief Alerts this resource that the simulation is about to enter
         * teardown. This call is the last opportunity for the resource to
         * access its parent node or any other resource that it did not manually
         * create. After this, the node will only be accessed by other nodes
         * receiving this message and to be deleted.
         * \note The Node owning this resource is alerted that teardown is
         * starting before this resource. The entire resource tree is notified
         * in depth-first ordered by construction between sublings
         * \note Expect this to always be invoked during normal usage.
         * \see TreeNode::onEnteringTeardown_
         */
        virtual void onStartingTeardown_();

        /*!
         * \brief A chance to bind local/subling/child resources after
         * finalization BEFORE the top-level Simulation gets a chance to bind
         * \note Called for each node during TreeNode::bindTreeEarly_ recursion
         */
        virtual void onBindTreeEarly_() {;}

        /*!
         * \brief A chance to bind local/subling/child resources after
         * finalization AFTER the top-level Simulation gets a chance to bind
         * \note Called for each node during TreeNode::bindTreeLate_ recursion
         */
        virtual void onBindTreeLate_() {;}

        /*!
         * \brief Resource container which owns this resource (if any)
         */
        TreeNode* res_container_;

        /*!
         * \brief The name of this Resource
         */
        std::string name_;

        /*!
         * \brief The clock this Resource belongs to
         */
        const Clock * clk_;
    };

    /*!
     * \brief Creates a helper traits class for determining whehther a type has
     * a member named getClock.
     */
    GENERATE_HAS_ATTR(getClock)

    /*!
     * \brief Helper for getting context of an assertion
     */
    class AssertContext
    {
        /*!
         * \brief just write the tick information to the stream
         */
        void appendTickData_(std::ostream& ss, Scheduler * sched)
        {
            if(sched) {
                ss << "tick: " << sched->getCurrentTick();
            }
            else {
                ss << "(no scheduler associated)";
            }
        }

        /*!
         * \brief write the clock and tick data to the stream.
         */
        void appendClockData_(std::ostream& ss, const Clock* clk)
        {
            Scheduler * sched = nullptr;
            if(clk){
                ss << "at cycle: " << clk->currentCycle() << " ";
                sched = clk->getScheduler();
            }else{
                ss << "(no clock associated) ";
            }
            // also always include the tick data along with the clock.
            appendTickData_(ss, sched);
        }
    public:


        /*!
         * \brief Handle default case where pointer is not a sparta::Resource
         */
        template <class CTXT>
        std::string getContextDescription(CTXT* ctxt,
                                          typename std::enable_if<!std::is_base_of<Resource, CTXT>::value
                                                                  && !std::is_base_of<TreeNode, CTXT>::value
                                                                  && !has_attr_getClock<CTXT>::value>::type* dummy = nullptr) {
            (void) ctxt;
            (void) dummy;
            std::stringstream ss;
            ss << "(from non-sparta context at ";
            appendTickData_(ss, nullptr);
            ss << ")";

            return ss.str();
        }

        /*!
         * \brief Handle case where pointer is a not subclass of (and not a) sparta::Resource, but
         * still has a getClock method. This method must be checked for its signature.
         * \note The signature of getClock could be verified in the template arguments, but
         * if the user created a non-resource class with a getClock method and then use the context
         * assert within it, they probably meant to actually get the clock rather than get a silly
         * message about not being inside a resource
         */
        template <class CTXT>
        std::string getContextDescription(CTXT* ctxt,
                                          typename std::enable_if<!std::is_base_of<Resource, CTXT>::value
                                                                  && !std::is_base_of<TreeNode, CTXT>::value
                                                                  && has_attr_getClock<CTXT>::value>::type* dummy = 0) {
            (void) dummy;
            std::stringstream ss;
            static_assert(std::is_pointer<decltype(ctxt->getClock())>::value,
                          "Type of this pointer is a class having a getClock method, but this "
                          "method does not return a pointer to a sparta::Clock");
            static_assert(std::is_base_of<sparta::Clock,
                                          typename std::remove_pointer<typename std::remove_cv<decltype(ctxt->getClock())>::type>::type>::value != false,
                          "Type of this pointer is a class having a getClock method, but this "
                          "method doe not return a sparta::Clock");
            auto clock = ctxt->getClock();
            // write the clock and tick to the stream.
            appendClockData_(ss, clock);

            return ss.str();
        }

        /*!
         * \brief Handle case where pointer is a subclass of (or is a) sparta::Resource
         */
        template <class CTXT>
        std::string getContextDescription(const sparta::Resource* ctxt,
                                          typename std::enable_if<std::is_base_of<Resource, CTXT>::value>::type* dummy = 0) {
            (void) dummy;
            std::stringstream ss;
            const ResourceContainer* rc = ctxt->getResourceContainer();
            if(!rc){
                ss << "(within uncontained resource)";
            }else{
                ss << "within resource at: " << rc->getLocation() << " ";
                const sparta::Clock* clock = ctxt->getClock();
                // write the clock and tick to the stream.
                appendClockData_(ss, clock);

            }
            return ss.str();
        }

        /*!
         * \brief Handle case where pointer is a subclass of (or is a) sparta::TreeNode
         */
        template <class CTXT>
        std::string getContextDescription(const sparta::TreeNode* ctxt,
                                          typename std::enable_if<std::is_base_of<TreeNode, CTXT>::value>::type* dummy = 0) {
            (void) dummy;
            std::stringstream ss;
            if(!ctxt){
                ss << "(within null treenode)"; // This should not happen, but could
            }else{
                ss << "within TreeNode: " << ctxt->getLocation() << " ";
                const sparta::Clock* clock = ctxt->getClock();
                // write the clock and tick to the stream.
                appendClockData_(ss, clock);
            }
            return ss.str();
        }
    };

    /*!
     * \brief Helper for adding context information to the end of a
     * SpartaException
     * \param ex sparta::Exception
     * \param thisptr this pointer from context of the call
     */
    #define ADD_CONTEXT_INFORMATION(ex, thisptr)  \
        sparta::AssertContext ac; \
        ex << " " << ac.getContextDescription<typename std::remove_pointer<decltype(thisptr)>::type>(thisptr);

    /*!
     * \brief Check condition and throw a sparta exception if condition evaluates
     * to false or 0. Exception will contain as much information about context
     * as this method can figure out. This includes Resources, TreeNodes, and
     * anyt type of this pointer having a getClock method.
     * \param e Condition
     * \param insertions code to insert additional strings to the end of a
     * sparta::Exception omitting the first '<<' operator. (e.g. "1 << 2")
     * \pre Must be used within a class where a this pointer is available
     * \pre If calling context is a subclass of sparta::TreeNode or
     * sparta::Resource, then it must publicly inherit from that class so that it
     * may access its getContainer, getLocation or getLocation methods as
     * appropriate
     *
     * Example
     * \code
     * sparta_assert_context(f==true, "Error at " << x << ", oh no!");
     * \endcode
     */
    #define sparta_assert_context(e, insertions)  \
        if(__builtin_expect(!(e), 0)) { sparta::SpartaException ex(std::string(#e) + ": " );  \
                                        ex << insertions;                                 \
                                        ADD_FILE_INFORMATION(ex, __FILE__, __LINE__);     \
                                        ADD_CONTEXT_INFORMATION(ex, this);                \
                                        throw ex; }


} // namespace sparta
// __RESOURCE__H__
#endif
