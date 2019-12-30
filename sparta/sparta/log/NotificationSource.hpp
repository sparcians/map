// <NotificationSource> -*- C++ -*-

#ifndef __NOTIFICATION_SOURCE_H__
#define __NOTIFICATION_SOURCE_H__

#include <iostream>
#include <vector>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/StringManager.hpp"
namespace sparta
{

    /*!
     * \brief Convenience macro for registering for a notification on a
     * NotificationSource
     * \param func Name of class member function that will be called when a
     * NotificationSource<NotificationDataT> posts its notification. This
     * function must have the signature:
     * \code
     * ClassName::func(const TreeNode& origin_node,      // TreeNode from which the notification originated
     *                 const TreeNode& observation_node, // TreeNode at which the notifications was observed, causing this callback
     *                 const datat& data)                // Notification event data
     * \endcode
     * \note Deregister with DEREGISTER_FOR_THIS. Failing to do so may cause
     * errors
     *
     * NotificationSources can be located throughout the simulation using
     * the sparta::TreeNode::locateNotificationSources method.
     *
     * This macro must be used within the class instance for which the callback
     * function is being registered. This macro uses the \a this pointer to
     * determine the observer class.
     *
     * This macro resolves to a function call to registerForThis which is
     * a member of sparta::NotificationSource.
     *
     * Example
     * \code
     * // ...
     * //NotificationSource<int> foo;
     * //TreeNode* node;
     * // ...
     * MyClass::setup() {
     *   node->REGISTER_FOR_THIS(handle_int);
     * }
     * MyClass::destroy() {
     *   node->DEREGISTER_FOR_THIS(handle_int);
     * }
     * MyClass::handle_int(const TreeNode& origin, const TreeNode& obs_pt, const int& data) {
     *   // ...
     * }
     * \endcode
     */
    #define REGISTER_FOR_THIS(func)                                         \
        registerForThis<typename std::remove_reference<decltype(*this)>::type, \
                        &std::remove_reference<decltype(*this)>::type::func>(this);

    /*!
     * \brief Convenience macro for deregistering for a notification on a TreeNode.
     *
     * \see REGISTER_FOR_NOTIFICATION
     */
    #define DEREGISTER_FOR_THIS(func)                                       \
        deregisterForThis<typename std::remove_reference<decltype(*this)>::type, \
                          &std::remove_reference<decltype(*this)>::type::func>(this);

    template<typename T>
    class MirrorNotificationSource;

    /*!
     * \brief A TreeNode that generates a single specific type of notification
     * which propagates up a tree of TreeNodes using TreeNode's builtin
     * functionality.
     * \note Subclassed by the tempalted NotificationSource class to provide
     */
    class NotificationSourceBase : public TreeNode
    {
    public:
        /**
         * This friendly relationship is required such that MirrorNotificationSource can call
         * our private methods for registration/deregistration that use tinfo instead of the
         * public templated types.
         */
        template <typename T>
        friend class MirrorNotificationSource;
        using TreeNode::DelegateVector;
        using TreeNode::delegate;

        //! \name Observation state change monitoring callbacks
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Various hook types for which one can register internal
         * callbacks on this notificaiton source.
         */
        enum class ObservationStateChange {
            SOLE_OBSERVER_REGISTERING,   //!< A sole observer is being registered to observe this notification source. This is a transition from 0 to 1 observers.
            SOLE_OBSERVER_DEREGISTERING, //!< A sole observer is being de-registered from observing this notification source. This is a transition from 1 to 0 observers.
            OBSERVER_REGISTERING,        //!< An observer is being registered to observe this notification source. This may be the first, 2nd, 3rd, or Nth observer.
            OBSERVER_DEREGISTERING       //!< An observer is being de-registered from observing this notification source. This may be the first, 2nd, 3rd, or Nth observer.
        };

        /*!
         * \brief Observation state change callback function signature
         * \param 0 Notification source for which the observer is being registered/deregistered
         * \param 1 Count of observers now observing the notification source (argument 0) after teh [de]registration causing this callback
         * \return void
         */
        typedef std::function<void (NotificationSourceBase const &, uint32_t)> callback_t;

        /*!
         * \brief Callback for observation-state of a NotificationSourceBase.
         *
         * Use this to monitor changes in the observation state of a
         * NotificationSource and bind some change to a callable. This object
         * automatically binds the callback function to the notification source
         * on construction and removes the callback on destruction. If the
         * notification source is destroyed before this callback object, then
         * the callback will never be invoked again and it will not attempt to
         * deregister when eventually destroyed.
         *
         * The intent of this feature is for some model to monitor the
         * observation of some of its constituent notifications for both their
         * lifetimes (which are hopefully the same). Creating and removing these
         * callbacks throughout a simulation is not expected. An example use
         * case is needing to change the state of a JIT model if someone is
         * now observing some detail of execution (e.g. an instruction executed
         * notification)
         *
         * \warning Do not attempt to create or delete callbacks within an
         * observation state change callback. It will fail. Do not attempt to
         * add or remove a NotificationObservers within a observation state
         * change callback!
         *
         * \verbatim
         * class X{
         * public:
         *   NotificationSource<MyPayload> ns;
         *   NotificationSource::ObservationStateCallback nscb;
         *   MyConstructor(node)
         *    : ns(node, ....),
         *      nscb(ns, NotificationSourceBase::ObservationStateChange::SOLE_OBSERVER_REGISTERING, handleSoleObserverAdded_)
         *   {}
         *   void handleSoleObserverAdded_(NotificationSourceBase const & source, uint32_t new_observer_count) {
         *   { ... }
         * }
         *
         * \endverbatim
         *
         * \todo Add a multi-callback version of this class that handles both
         * sole-[de]registering functions and both any-[de]registering functions
         *
         * \todo Aoply this pattern (with code re-use) to Counter types as well
         * so that Counter-incrementing code can change it's behavior and
         * granularity to better support counter-based SPARTA tirggers
         */
        class ObservationStateCallback {

            ObservationStateChange type_;
            callback_t callback_;
            TreeNode::ConstWeakPtr tree_node_weak_ptr_;
            NotificationSourceBase& ns_;

        public:

            ObservationStateCallback(NotificationSourceBase& ns,
                                     ObservationStateChange type,
                                     callback_t callback)
              : type_(type),
                callback_(callback),
                tree_node_weak_ptr_(ns.getWeakPtr()),
                ns_(ns)
            {
                ObservationStateCallbackPrivateInterface::registerObservationStateChangeCallback_(ns_, this);
            }

            ~ObservationStateCallback()
            {
                // Deregister from notification ONLY if the node has not been destroyed already
                if (tree_node_weak_ptr_.expired() == false) {
                    ObservationStateCallbackPrivateInterface::deregisterObservationStateChangeCallback_(ns_, this);
                }
            }

            ObservationStateCallback(const ObservationStateCallback&) = delete;
            ObservationStateCallback(ObservationStateCallback&&) = delete;
            ObservationStateCallback& operator= (const ObservationStateCallback&) = delete;

            /*!
             * \brief Get tyhe type of change associated with this callback instance
             */
            ObservationStateChange getType() const { return type_; }

            /*!
             * \brief Invoked the callback function
             */
            void operator()(NotificationSourceBase const & ns,
                            uint32_t observers) const
            {
                callback_(ns, observers);
            }

            /*!
             * \brief Dump string description to an ostream
             */
            void dump(std::ostream& o) const {
                o << "<ObservationStateCallback type=" << static_cast<int>(type_) << " node=";
                if(tree_node_weak_ptr_.expired()){
                    o << "EXPIRED";
                }else{
                    o << tree_node_weak_ptr_.lock()->getLocation();
                }
                o << ">";
            }
        };

        /*!
         * \brief Controlled interface to NotificationSourceBase for
         * regiistering and deregistering observation state change callback objects only
         */
        class ObservationStateCallbackPrivateInterface {
        private:
            static void registerObservationStateChangeCallback_(NotificationSourceBase & ns,
                                                                ObservationStateCallback* const cbobject)
            {
                ns.registerObservationStateChangeCallback_(cbobject);
            }


            static void deregisterObservationStateChangeCallback_(NotificationSourceBase & ns,
                                                                  ObservationStateCallback* const cbobject)
            {
                ns.deregisterObservationStateChangeCallback_(cbobject);
            }

            friend class ObservationStateCallback; //!< \brief Allow controlled interaction with this class
        };

        friend class ObservationStateCallbackInterface; //!< \brief Allow controlled interaction with some members of this class

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief NotificationSourceBase
         * \param parent parent node. Must not be nullptr.
         * \param name Name of this node. Must be a valid TreeNode name
         * \param group Group of this node. Must be a valid TreeNode group
         * when paired with group_idx.
         * \param group_idx Group index. Must be a valid TreeNode group_idx
         * when paired with group.
         * \param desc Description of this node. Required to be a valid
         * TreeNode description.
         * \param notification_name_id Pointer to string interned in
         * sparta::StringManager::internString representing the name of the
         * notification that will be posted by this node. Must satisfy
         * validateNotificationName()
         * \param notification_tinfo typeid of notification payload that will be
         * posted by this node. This node must not post its own notifications
         * with payload types other than this.
         */
        NotificationSourceBase(TreeNode* parent,
                               const std::string& name,
                               const std::string& group,
                               TreeNode::group_idx_type group_idx,
                               const std::string& desc,
                               const std::string* notification_name_id,
                               const std::type_info& notification_tinfo) :
            TreeNode(name, group, group_idx, desc),
            noti_id_(notification_name_id),
            noti_tinfo_(notification_tinfo),
            observed_(false),
            num_posts_(0)
        {
            if(nullptr == parent){
                throw SpartaException("NotificationSourceBase ")
                    << getLocation() << " must be constructed with a parent";
            }

            setExpectedParent_(parent);

            if(nullptr == notification_name_id){
                throw SpartaException("NotificationSourceBase ")
                    << getLocation() << " cannot be constructed with a null notification_name_id";
            }

            // Other initialization here
            // ...

            validateNotificationName(*notification_name_id);

            parent->addChild(this);

            determineObs_Nodes_();
        }

        /*!
         * \brief Alternate construct that accepts a notification name string
         * instead of an interned string pointer.
         */
        NotificationSourceBase(TreeNode* parent,
                               const std::string& name,
                               const std::string& group,
                               TreeNode::group_idx_type group_idx,
                               const std::string& desc,
                               const std::string& notification_name,
                               const std::type_info& notification_tinfo) :
            NotificationSourceBase(parent, name, group, group_idx, desc,
                                   StringManager::getStringManager().internString(notification_name),
                                   notification_tinfo)
        { }


        /*!
         * \brief Destructor
         */
        virtual ~NotificationSourceBase() {}

        /*!
         * \brief Is this NotificationSourceBase being observed at this node or an
         * ancestor of any distance.
         */
        bool observed() const noexcept {
            return observed_;
        }

        /*!
         * \brief Validates the given notification name string
         * \throw SpartaException if the name is invalid.
         *
         * Constraints:
         * \li Name must not exactly match any names in sparta::RESERVED_WORDS
         * \li Name must not begin with a decimal digit
         * \li Name must contain only alphanumeric characters and underscores
         * (see sparta::ALPHANUM_CHARS).
         */
        static void validateNotificationName(const std::string& nm)
        {
            for(const char* rsrv : RESERVED_WORDS){
                if(nm == rsrv){
                    throw SpartaException("Notification name \"")
                        << nm << "\" is a reserved word. ";
                }
            }

            if(nm.size() > 0 && nm[0] == '_'){
                throw SpartaException(" Notification name \"")
                    << nm << "\" begins with an '_' which is not permitted";
            }

            if(nm.find("__") != std::string::npos){
                throw SpartaException(" Notification name \"")
                    << nm << "\" contains two adjacent underscores which is not permitted";
            }

            if(nm.find_first_of(DIGIT_CHARS) == 0){
                throw SpartaException(" Notification name \"")
                    << nm << "\" begins with a '" << nm[nm.size()-1] << "' character which is not permitted. "
                    << "A Notification name must not begin with a decimal digit.";
            }

            size_t pos = nm.find_first_not_of(ALPHANUM_CHARS);
            if(pos != std::string::npos){
                throw SpartaException("Notification name \"")
                    << nm << "\" contains a '" << nm[pos] << "', which is not permitted. "
                    << "A Notification name must contain only alphanumeric characters and underscores.";
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Properties
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Returns notification ID (string pointer from Notification ID
         * interned in sparta::StringManager)
         *
         * Allows fast comparison by pointer instead of string.
         */
        const std::string* getNotificationID() const {
            return noti_id_;
        }

        /*!
         * \brief Returns the notification name string for slow string
         * comparison or printing.
         */
        const std::string& getNotificationName() const {
            return *noti_id_;
        }

        /*!
         * \brief Returns std::type_info from typeid() on the notification
         * data type which this NotificationSource can emit.
         */
        const std::type_info& getNotificationType() const {
            return noti_tinfo_;
        }

        /*!
         * \brief Gets the demangled name of the C++ notification type which
         * this NotificationSource can emit.
         * \see getNotificationType
         */
        const std::string getNotificationTypeName() const {
            return demangle(noti_tinfo_.name());
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        /*!
         * \brief Returns the vector of observation points on this notification
         * source.
         * \note This is subject to change as observers are added and removed.
         *
         * This is NOT the set of observers that will receive this
         * notification, but the number of TreeNodes (observation points) with 1
         * or more observers for this notification.
         */
        const std::vector<TreeNode*>& getObservationPoints() const {
            return obs_nodes_;
        }

        /*!
         * \brief Returns the number of observation points affecting this
         * notification source.
         * \note This is subject to change as observers are added and removed.
         *
         * This is NOT the number of observers that will receive this
         * notification, but the number of TreeNodes (observation points) with 1
         * or more observers for this notification.
         */
        uint32_t getNumObservationPoints() const {
            return obs_nodes_.size();
        }

        /*!
         * \brief Returns the number of delegates affected by postings of this
         * notification
         * \note This is subject to change as observers are added and removed.
         */
        uint32_t getNumObservers() const {
            return dels_.size();
        }

        /*!
         * \brief Returns the number of notifications posted by this node
         */
        uint64_t getNumPosts() const {
            return num_posts_;
        }

    protected:

        const std::string* noti_id_;       //!< Name of notification generated. Should be similar to node name
        const std::type_info& noti_tinfo_; //!< Type of notification data

        /*!
         * \brief Vector of TreeNodes at which the notification that this node
         * can generate is being observed.
         */
        std::vector<TreeNode*> obs_nodes_;

        /*!
         * \brief Vector of delegates directly observing this notification
         * source.
         *
         * These are copies of delegates registered at this TreeNode or any
         * ancestor node. When posting this notification, these delegates can be
         * directly invoked
         */
        std::vector<delegate> dels_;

        bool observed_; //!< Cached value of obs_nodes_.size() > 0 for faster queries

        /*!
         * \brief Number of messages posted whether observed or not
         * \note Mutable so that posting can be done throug const methods
         */
        mutable uint64_t num_posts_;

        // Override from TreeNode
        //! \post Updates observed status
        virtual void notificationObserverAdded_(const std::type_info& tinfo,
                                                const std::string* name_id,
                                                TreeNode* obs_node,
                                                const delegate* del) override {
            // Expected to only see observation on the notification that this source can
            // generate. If these assertions are hit, then the framework is broken or
            // canGenerateNotification_ is incorrect
            sparta_assert(tinfo == noti_tinfo_);
            sparta_assert(name_id == noti_id_ || name_id == StringManager::getStringManager().EMPTY);

            // Add the observation point node if unique
            auto itr = std::find(obs_nodes_.begin(), obs_nodes_.end(), obs_node);
            if(itr == obs_nodes_.end()){
                obs_nodes_.push_back(obs_node);
            }

            dels_.push_back(*del);

            const bool was_observed = observed_;
            observed_ = true;

            // Callback happen after state changes are complete (or before) in order to allow recursion
            if(not was_observed){
                invokeObservationStateChangeCallbacks_(ObservationStateChange::SOLE_OBSERVER_REGISTERING);
            }
            invokeObservationStateChangeCallbacks_(ObservationStateChange::OBSERVER_REGISTERING);
        }

        // Override from TreeNode
        //! \post Updates observed status
        virtual void notificationObserverRemoved_(const std::type_info& tinfo,
                                                  const std::string* name_id,
                                                  TreeNode* obs_node,
                                                  const delegate* del) override {
            // Expected to only see observation on the notification that this source can
            // generate. If these assertions are hit, then the framework is broken or
            // canGenerateNotification_ is incorrect
            sparta_assert(tinfo == noti_tinfo_);
            sparta_assert(name_id == noti_id_ || name_id == StringManager::getStringManager().EMPTY);

            const bool was_observed = observed_;

            auto delitr = std::find(dels_.begin(), dels_.end(), *del);
            if(delitr != dels_.end()){
                dels_.erase(delitr);
            }

            observed_ = dels_.size() > 0;

            // Remove the observation point node if no delegates refer to it any more
            bool remaining = false;
            for(auto d : dels_){
                if(d.getObservationPoint() == obs_node){
                    remaining = true;
                    break;
                }
            }
            if(false == remaining){
                auto itr = std::find(obs_nodes_.begin(), obs_nodes_.end(), obs_node);
                if(itr != obs_nodes_.end()){
                    obs_nodes_.erase(itr);
                }
            }

            // Callback happen after state changes are complete (or before) in order to allow recursion
            // Reverse order of callback from notificationObserverAdded_.
            invokeObservationStateChangeCallbacks_(ObservationStateChange::OBSERVER_DEREGISTERING);
            if(was_observed && not observed_){
                invokeObservationStateChangeCallbacks_(ObservationStateChange::SOLE_OBSERVER_DEREGISTERING);
            }
        }

        // Override from TreeNode
        // Returns true only for matches on node's notification ID or the input pattern
        bool canGenerateNotification_(const std::type_info& tinfo,
                                      const std::string* id,
                                      const std::string*& match) const override {
            if(noti_tinfo_ != tinfo && tinfo != typeid(sparta::TreeNode::ANY_TYPE)){
                return false;
            }

            if(notificationCategoryMatch(id, noti_id_)){
                match = noti_id_;
                return true;
            }

            return false;
        }

        // Override from TreeNode
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            std::stringstream ss;
            ss << '<' << getLocation() << " name:\""
               << getNotificationName() << "\" datat:(" << getNotificationTypeName()
               << ") " << " observers:" << getNumObservers()
               << " posted:" << getNumPosts() << '>';
            return ss.str();
        }
    private:

        // Override from TreeNode
        virtual void onAddedAsChild_() noexcept override final {
            determineObs_Nodes_();
        }


        /*!
         * \brief Determines the full set of observers of this
         * NotificationSourceBase by ascending the tree and querying all ancestors
         * including the global virtual node.
         * \post Makes obs_nodes_ current
         *
         * Invoke this method when this node is part of a subtree which is added
         * as a child of another node.
         */
        void determineObs_Nodes_() {
            obs_nodes_.clear();
            dels_.clear();
            observed_ = false;

            TreeNode* node = this;
            while(node){
                node->getDelegatesRegisteredForNotification(noti_tinfo_, noti_id_, dels_);
                if(node->hasObserversRegisteredForNotification(noti_tinfo_, noti_id_)){
                    obs_nodes_.push_back(node);
                }
                node = node->getParent();
            }

            node = getVirtualGlobalNode();
            node->getDelegatesRegisteredForNotification(noti_tinfo_, noti_id_, dels_);
            if(node->hasObserversRegisteredForNotification(noti_tinfo_, noti_id_)){
                obs_nodes_.push_back(node);
            }

            // Update observed flag
            observed_ = dels_.size() > 0;
        }

        // Override from TreeNode
        // Adds this node's notification info
        void getPossibleNotifications_(std::vector<NotificationInfo>& infos) const override {
            infos.emplace_back(this, &noti_tinfo_, noti_id_);
        }

        /*!
         * \brief Register an observation state change callback with this Notification
         */
        void registerObservationStateChangeCallback_(ObservationStateCallback* const hook) {
            // User errors
            sparta_assert(in_observation_state_change_callback_ == false,
                        "cannot add observation state change callbacks from within a callback");

            // Internal errors
            sparta_assert(hook); // Internal-only hook argument is null somehow?
            sparta_assert(std::find(obs_state_change_cbs_.begin(), obs_state_change_cbs_.end(), hook) == obs_state_change_cbs_.end(),
                        "Internal notification hook " << *hook
                        << " was installed a second time to notification source " << *this);

            // Track it in a list
            obs_state_change_cbs_.emplace_back(hook);
        }

        /*!
         * \brief Deregister an observation state change callback with this Notification
         */
        void deregisterObservationStateChangeCallback_(ObservationStateCallback* const hook) {
            // User errors
            sparta_assert(in_observation_state_change_callback_ == false,
                        "cannot remove observation state change callbacks from within a callback");

            const auto itr = std::find(obs_state_change_cbs_.begin(), obs_state_change_cbs_.end(), hook);

            // Internal error checking
            sparta_assert(itr != obs_state_change_cbs_.end(),
                        "Internal notification hook " << *hook
                        << " was not found  while attemptint to deregister it from notification source " << *this);

            // Remove it so it is no longer called
            obs_state_change_cbs_.erase(itr);
        }

        /*!
         * \brief Invoke all observation state change callbacks of a given type
         * \warning, this does not support modification of the callback list
         * from within the list.
         */
        void invokeObservationStateChangeCallbacks_(ObservationStateChange to_call) {
            // User Error
            sparta_assert(in_observation_state_change_callback_ == false,
                        "cannot recursively invoke observation state change callbacks from within a "
                        "callback. User may be changing Notification Observers in response to "
                        "changes in observation state. This is not allowed");

            in_observation_state_change_callback_ = true;

            // Break on first exception!
            try {
                for(ObservationStateCallback const * cb : obs_state_change_cbs_){
                    if(cb->getType() == to_call){
                        (*cb)(*this, getNumObservers());
                    }
                }
            } catch (...) {
                in_observation_state_change_callback_ = false;

                throw; // Re-throw
            }

            in_observation_state_change_callback_ = false;
        }

        /*!
         * \brief Manage observation state change callbacks. This is a simple
         * vector becuse it contains borrowed references these callbacks are
         * not performance sensitive. Changes to the observation of a node is
         * an instrumentation change and not expected to be common. Very few
         * callbacks are expected to be registed from any one NotificationSource
         * anyway.
         */
        std::vector<ObservationStateCallback*> obs_state_change_cbs_;

        /*!
         * \brief Currently in an observation state change callback?
         *
         * Detects callback recursion and prevents modiciation of callback list
         * during callbacks because it would cause errors. The need for this
         * variable *could* be eliminated but it would require more work and
         * goes against the expected use of this feature.
         */
        bool in_observation_state_change_callback_ = false;
    };


    /*!
     * \brief A TreeNode that generates a specific type of notification which
     * propagates up a tree of TreeNodes (using the NotificationPropagator base
     * class.
     * \param NotificationDataT Type of data object provided to
     * notification observers. Must NOT be const, volatile, pointer, or
     * reference.
     */
    template <typename NotificationDataT>
    class NotificationSource : public NotificationSourceBase
    {
    public:

        typedef NotificationDataT data_type; //!< Type of notification data generated by this instance
        typedef NotificationDataT const const_data_type; //!< Const qualified data type

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief NotificationSource
         * \param parent parent node. Must not be nullptr.
         * \param name Name of this node. Must be a valid TreeNode name
         * \param group Group of this node. Must be a valid TreeNode group
         * when paired with group_idx.
         * \param group_idx Group index. Must be a valid TreeNode group_idx
         * when paired with group.
         * \param desc Description of this node. Required to be a valid
         * TreeNode description.
         * \param notification_name_id Name of this notification interned in
         */
        NotificationSource(TreeNode* parent,
                           const std::string& name,
                           const std::string& group,
                           TreeNode::group_idx_type group_idx,
                           const std::string& desc,
                           const std::string* notification_name_id) :
            NotificationSourceBase(parent, name, group, group_idx,
                                   desc, notification_name_id,
                                   typeid(NotificationDataT))
        {
            static_assert(std::is_same<
                            typename std::remove_cv<
                              typename std::remove_reference<
                                typename std::remove_pointer<NotificationDataT>::type
                              >::type
                            >::type,
                            NotificationDataT
                          >::value,
                          "NotificationDataT must NOT be a const, volatile, pointer, or reference"
                          "type. It violates at least one of these restrictions");

            // Other initialization here
            // ...
        }

        // Alternate constructor
        NotificationSource(TreeNode* parent,
                           const std::string& name,
                           const std::string& group,
                           TreeNode::group_idx_type group_idx,
                           const std::string& desc,
                           const std::string& notification_name) :
            NotificationSource(parent,
                               name,
                               group,
                               group_idx,
                               desc,
                               StringManager::getStringManager().internString(notification_name))
        {
            // Initialization handled in constructor delegation
        }

        // Alternate constructor
        NotificationSource(TreeNode* parent,
                           const std::string& name,
                           const std::string& desc,
                           const std::string* notification_name_id) :
            NotificationSource(parent,
                               name,
                               TreeNode::GROUP_NAME_NONE,
                               TreeNode::GROUP_IDX_NONE,
                               desc,
                               notification_name_id)
        {
            // Initialization handled in constructor delegation
        }

        // Alternate constructor
        NotificationSource(TreeNode* parent,
                           const std::string& name,
                           const std::string& desc,
                           const std::string& notification_name) :
            NotificationSource(parent,
                               name,
                               TreeNode::GROUP_NAME_NONE,
                               TreeNode::GROUP_IDX_NONE,
                               desc,
                               StringManager::getStringManager().internString(notification_name))
        {
            // Initialization handled in constructor delegation
        }

        // Alternate constructor with anonymous name
        NotificationSource(TreeNode* parent,
                           const std::string& group,
                           TreeNode::group_idx_type group_idx,
                           const std::string& desc,
                           const std::string* notification_name_id) :
            NotificationSource(parent,
                               "",
                               group,
                               group_idx,
                               desc,
                               notification_name_id)
        {
            // Initialization handled in constructor delegation
        }

        // Alternate constructor with anonymous name
        NotificationSource(TreeNode* parent,
                           const std::string& group,
                           TreeNode::group_idx_type group_idx,
                           const std::string& desc,
                           const std::string& notification_name) :
            NotificationSource(parent,
                               "",
                               group,
                               group_idx,
                               desc,
                               StringManager::getStringManager().internString(notification_name))
        {
            // Initialization handled in constructor delegation
        }

        /*!
         * \brief Destructor
         * \note NotificationSource is not intended to be overloaded
         */
        ~NotificationSource() {}



        //! \name Notification Interface
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Registers a callback method to listen for the notification
         * generated by this NotificationSource.
         * function registered. This type must be a copyable C++ typename which
         * is non-const, non-reference, and non-pointer.
         * \tparam T Class of object on which callback member function will be
         * invoked
         * \tparam TMethod member function pointer of class \a T that will be
         * invoked when the specified notification is posted. This member
         * function signature must be:
         *
         * The REGISTER_FOR_THIS macro conveniently wraps this function call for
         * use within a class member function
         *
         * Regisration through this method should be removed through
         * deregisterForThis before \a obj is destroyed
         *
         * NotificationSources can be located throughout the simulation using
         * the sparta::TreeNode::locateNotificationSources method.
         *
         * This method uses sparta::TreeNode::registerForNotification to actually
         * register. Refer to this method for more details about other parameters
         *
         * Example:
         * \code
         * struct MyClass {
         *     void func1(const TreeNode& origin_node,      // TreeNode from which the notification originated
         *                const TreeNode& observation_node, // TreeNode at which the notifications was observed, causing this callback
         *                const datat& data) {}             // Notification event data
         *     void func2(const datat& data) {}
         * }
         * // ...
         * MyClass my_class;
         * // ...
         * // Given NotificationSource<struct SomeData> node;
         * node->registerForThis<MyClass, &MyClass::func1>(my_class);
         * node->registerForThis<MyClass, &MyClass::func2>(my_class);
         * // ...
         * node->deregisterForThis<MyClass, &MyClass::func1>(my_class);
         * node->deregisterForThis<MyClass, &MyClass::func2>(my_class);
         * \endcode
         */
        template <typename T, void (T::*TMethod)(const TreeNode&, const TreeNode&, const_data_type&)>
        void registerForThis(T* obj) {
            sparta::TreeNode::registerForNotification<data_type, T, TMethod>(obj, *noti_id_);
        }

        // Overload accepting a class member function signature having only a data argument
        template <typename T, void (T::*TMethod)(const_data_type&)>
        void registerForThis(T* obj) {
            sparta::TreeNode::registerForNotification<data_type, T, TMethod>(obj, *noti_id_);
        }

        /*!
         * \brief Deregisters a callback method that was registered with
         * registerForThis.
         *
         * This method uses sparta::TreeNode::deregisterForNotification to
         * actually deregister. Refer to this method for more details about
         * other parameters.
         */
        template <typename T, void (T::*TMethod)(const TreeNode&, const TreeNode&, const_data_type&)>
        void deregisterForThis(T* obj) {
            sparta::TreeNode::deregisterForNotification<data_type, T, TMethod>(obj, *noti_id_);
        }

        // Overload accepting a class member function signature having only a data argument
        template <typename T, void (T::*TMethod)(const_data_type&)>
        void deregisterForThis(T* obj) {
            sparta::TreeNode::deregisterForNotification<data_type, T, TMethod>(obj, *noti_id_);
        }

        /*! \brief Post with reference to data with parent as message origin.
         * \param data Reference to data to post. Must be valid for duration
         * of this call
         */
        void postNotification(const NotificationDataT& data) const {
            ++num_posts_;

            // Could notify observers at this node and above through TreeNode's propagation interface
            //sparta_assert(getParent() != nullptr,
            //                  "Cannot postNotification from NotificationSource " << getLocation() << " because parent is null");
            //postPropagatingNotification_(this, data, noti_id_);

            // Could Notify all observing TreeNodes and allow them to invoke delegates
            //for(TreeNode* o_node : obs_nodes_){
            //    invokeDelegatesOn_(o_node, data, noti_id_);
            //}

            // Directly invoke all applicable delegates.
            // Note that this works even if ancestors are destroyed because
            // deregistration does not take place.
            for(const delegate& d : dels_){
                d(*this, data);
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}
    };

    /**
     * \class MirrorNotificationSource
     * \brief essentially a pass through notification source that is a placeholder
     *        in a shadow tree.
     *
     * This is a placeholder to represent a potentially private notification source or many
     * private notification sources originating from this location in the tree.
     */
    template<typename NotificationDataT>
    class MirrorNotificationSource : public NotificationSourceBase
    {
        typedef std::vector<NotificationSourceBase*> SrcList;
    public:
        /*!
         * \brief MirrorNotificationSource
         * \param parent parent node. Must not be nullptr.
         * \param name Name of this node. Must be a valid TreeNode name
         * \param group Group of this node. Must be a valid TreeNode group
         * when paired with group_idx.
         * \param group_idx Group index. Must be a valid TreeNode group_idx
         * when paired with group.
         * \param desc Description of this node. Required to be a valid
         * TreeNode description.
         * \param notification_name_id Pointer to string interned in
         * sparta::StringManager::internString representing the name of the
         * notification that will be posted by this node. Must satisfy
         * validateNotificationName()
         */
        MirrorNotificationSource(TreeNode* parent,
                                 const std::string& name,
                                 const std::string& group,
                                 TreeNode::group_idx_type group_idx,
                                 const std::string& desc,
                                 const std::string* notification_name_id):
            NotificationSourceBase(parent, name, group, group_idx, desc,
                                   notification_name_id,
                                   typeid(NotificationDataT))
        {}

        /*!
         * \brief Alternate construct that accepts a notification name string
         * instead of an interned string pointer.
         */
        MirrorNotificationSource(TreeNode* parent,
                                 const std::string& name,
                                 const std::string& group,
                                 TreeNode::group_idx_type group_idx,
                                 const std::string& desc,
                                 const std::string& notification_name) :
            NotificationSourceBase(parent, name, group, group_idx,
                                   desc,
                                   notification_name,
                                   typeid(NotificationDataT))
        {}

        // Alternate constructor
        MirrorNotificationSource(TreeNode* parent,
                           const std::string& name,
                           const std::string& desc,
                           const std::string& notification_name) :
            MirrorNotificationSource(parent,
                                     name,
                                     TreeNode::GROUP_NAME_NONE,
                                     TreeNode::GROUP_IDX_NONE,
                                     desc,
                                     StringManager::getStringManager().internString(notification_name))
        {}

        /**
         * Given a node in potentially private treenodes, bind to all
         * notification sources under it that match our type and name.
         *
         * The node may actually be the concrete
         * NotificationSource or it can be a higher node up.
         *
         * The idea is that a single mirror could reflect notifications of same type/name
         * coming from multiple sources under a common node. So we could bind to the node
         * above all of those sources to achieve that.
         */
        void addLink(TreeNode *node, const std::string &label) override final
        {
            (void)label; // We do not care about the label.
            sparta_assert(node != nullptr);
            const uint32_t prev_size = concretes_.size();
            findMatchingSourcesBelowNode_(*node, concretes_);
            // Make sure we found atleast 1 node that we are shadowing,
            // otherwise this MirrorNotificationSource is not linked to
            // anyone and should be taken out.
            if (concretes_.size() - prev_size == 0)
            {
                SpartaException ex("MirrorNotificationSource \"");
                ex << getName() << "\" was unable to bind to any real NotificationSources"
                   << " found under the nodes provided to addLink().";
                throw ex;
            }
        }
    private:

        /**
         * see if we can find any notification sources at or below node
         * and return a list of them.
         */
        void findMatchingSourcesBelowNode_(sparta::TreeNode& node,
                                           SrcList& bases)
        {
            auto* as_src = dynamic_cast<NotificationSourceBase*>(&node);
            if (as_src)
            {
                // If this MirrorNotificationSource happened to live under the search
                // of node, then we would certainly not want to add ourselves as a
                // concrete instance!
                if (as_src != this)
                {
                    // We won't mirror anything that isn't explicitly allowed by the concrete
                    // source.
                    if (as_src->getNotificationID() == getNotificationID() &&
                        as_src->getNotificationType() == getNotificationType())
                    {
                        bases.emplace_back(dynamic_cast<NotificationSourceBase*>(&node));
                    }
                }
            }

            for (auto n : node.getChildren())
            {
                findMatchingSourcesBelowNode_(*n, bases);
            }

        }
        // Override from TreeNode
        //! \post Updates observed status
        void notificationObserverAdded_(const std::type_info& tinfo,
                                        const std::string* name_id,
                                        TreeNode* obs_node,
                                        const delegate* del) override final
        {
            if (del->revealsOrigin())
            {
                SpartaException ex("Cannot register for notification \"");
                ex << *name_id << "\"" << " because this notification source "
                   << "is only shadowing a private concrete notification source. "
                   << "The callback signature type would reveal access to a private node "
                   << "and be misleading. We do not support this feature at this time.";
                throw ex;
            }

            if (concretes_.size() == 0)
            {
                SpartaException ex ("Cannot register for notification \"");
                ex << *name_id << "\" because this is a MirrorNotificationSource "
                   << "that is not linked to any concrete sources. It is possible you "
                   << "are registering for the notification too early and should be registering "
                   << "during onBindTreeLate_()";
                throw ex;
            }
            for(auto concrete : concretes_)
            {
                concrete->notificationObserverAdded_(tinfo, name_id,
                                                     obs_node, del);
            }
        }
        // Override from TreeNode
        //! \post Updates observed status
        void notificationObserverRemoved_(const std::type_info& tinfo,
                                                  const std::string* name_id,
                                                  TreeNode* obs_node,
                                                  const delegate* del) override final
        {
            if (concretes_.size() == 0)
            {
                SpartaException ex ("Cannot deregister for notification \"");
                ex << *name_id << "\" because this is a MirrorNotificationSource "
                   << "that is not linked to any concrete sources. It is possible you "
                   << "are deregistering for the notification too early and should be registering "
                   << "during onBindTreeLate_()";
                throw ex;
            }

            for(auto concrete : concretes_)
            {
                concrete->notificationObserverRemoved_(tinfo, name_id,
                                                       obs_node, del);
            }
        }
        // Override from TreeNode
        // Returns true only for matches on node's notification ID or the input pattern
        bool canGenerateNotification_(const std::type_info& tinfo,
                                      const std::string* id,
                                      const std::string*& match) const override final
        {
            if (concretes_.size() == 0)
            {
                return false;
            }
            bool can_generate = true;
            for (auto noti_src : concretes_)
            {
                can_generate &= noti_src->canGenerateNotification_(tinfo, id, match);
            }
            return can_generate;
        }

        /**
         * Since we are a shadow treenode, our validation is that we
         * atleast have 1 notification source that we are shadowing and
         * that the sources we shadow are all of the same type and name as
         * us.
         */
        void validateNode_() const override final
        {
            if (concretes_.size() == 0)
            {
                SpartaException ex("Unbound MirrorNotificationSource ");
                ex << getName() << " was never bound to concrete notification sources.";
                throw ex;
            }
        }

        // Override from TreeNode
        virtual std::string stringize(bool pretty=false) const override final {
            (void) pretty;
            std::stringstream ss;
            ss << NotificationSourceBase::stringize(pretty);
            ss << " shadows: ";
            try{
                ss << concretes_.size() << " sources ";
            }
            catch (SpartaException&)
            {
                ss << "[UNAVAILABLE]";
            }
            return ss.str();
        }
        //! A list of notifications that we mirror.
        SrcList concretes_;
    };

    inline std::ostream& operator<<(std::ostream& o, const NotificationSourceBase::ObservationStateCallback& osc)
    {
        osc.dump(o);
        return o;
    }

    inline std::ostream& operator<<(std::ostream& o, const NotificationSourceBase::ObservationStateCallback* osc)
    {
        if(osc == nullptr){
            o << "nullptr";
        }else{
            o << *osc;
        }
        return o;
    }

} // namespace sparta

// __NOTIFICATION_SOURCE_H__
#endif
