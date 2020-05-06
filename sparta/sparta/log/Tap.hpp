// <Tap> -*- C++ -*-

#pragma once

#include <iostream>
#include <vector>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/categories/CategoryManager.hpp"
#include "sparta/utils/StringManager.hpp"
#include "sparta/log/Destination.hpp"
#include "sparta/log/Events.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
namespace sparta
{
    namespace log
    {

        /*!
         * \brief Logging Tap. Attach to a TreeNode to intercept logging
         * messages from any NotificationSource nodes in the subtree of that
         * node (including the node itself).
         * \note The creation and destruction of Taps is not thread-safe.
         * Construction and destruction must be protected.
         * \note noncopyable
         */
        class Tap
        {
        public:

            //! Disallow copy construction
            Tap(const Tap&) = delete;

            //! Disallow copy assignment
            Tap& operator=(const Tap&) = delete;

            /*!
             * \brief Constructor
             * \tparam DestT Destination type
             * \param node Node that this tap will observe. Must not be nullptr
             * \param pcategory Category on which this node will filter. Must be
             * interned in StringMananger. If the category string is not already
             * known to be interned, use the alternate constructor for Tap.
             * \param dest Detination identifier (e.g. std::string filename or
             * ostream&). See sparta::log::Destination and Destination.h for
             * possible types.
             *
             * Example:
             * \code
             * Tap t(root.getChild("a.b.c"), sparta::StringManager::getStringManager().EMPTY, "out.log"); // Opens for write once per session
             * Tap t(root.getChild("a.b.c"), sparta::log::categories::WARN, std::cerr); // Opens for write once per session
             * Tap t(root.getChild("a.b.c"), sparta::StringManager::getStringManager().internString("hello"), "out.log"); // Opens for write once per session
             * \endcode
             */
            template <typename DestT>
            Tap(TreeNode* node, const std::string* pcategory, DestT& dest) :
                node_(node),
                category_(pcategory)
            {
                Destination* d = DestinationManager::getDestination(dest); // Can instantiate new
                sparta_assert(d != nullptr);
                dest_ = d;

                if(nullptr == node){
                    throw SpartaException("Cannot attach a Tap to a null TreeNode");
                }

                reset(node);
            }

            /*!
             * \brief Constructor
             *
             * Example:
             * \code
             * Tap t(root.getChild("a.b.c"), "",        "out.log"); // Opens for write once per session
             * Tap t(root.getChild("a.b.c"), "warning", std::cerr); // Opens for write once per session
             * Tap t(root.getChild("a.b.c"), "hello",   "out.log"); // Opens for write once per session
             * \endcode
             */
            template <typename DestT>
            Tap(TreeNode* node, const std::string& category, DestT& dest) :
                Tap(node, StringManager::getStringManager().internString(category), dest)
            {
                // Initialization handled in delegated constructor
            }

            /*!
             * \brief Non-observing Constructor
             *
             * Category and destination must be defined here, but node at which
             * to observe can be specified later via reset.
             *
             * Example:
             * \code
             * Tap t("",        "out.log"); // Opens for write once per session
             * Tap t("warning", std::cerr); // Opens for write once per session
             * Tap t("hello",   "out.log"); // Opens for write once per session
             * t.reset(root.getChild("a.b.c");
             * \endcode
             */
            template <typename DestT>
            Tap(const std::string& category, DestT& dest) :
                node_(nullptr),
                category_(StringManager::getStringManager().internString(category))
            {
                Destination* d = DestinationManager::getDestination(dest); // Can instantiate new
                sparta_assert(d != nullptr);
                dest_ = d;
            }

            /*!
             * \brief Destructor
             * \note Does not affect destination
             */
            virtual ~Tap() {
                detach();
            }

            /*!
             * \brief Detach the tap from the current node (if any) and
             * re-attach to a new node. Destination and category of observation
             * is maintained.
             * \param node The node to attach to. If null, this call effectively
             * behaves like detach.
             * \note Destination and category were set in constructor
             */
            void reset(TreeNode* node) {
                detach();

                if(nullptr != node){
                    node_wptr_ = node->getWeakPtr();

                    // Use the form of the registration command which can ignore the no-notification-source case
                    TreeNodePrivateAttorney::registerForNotification<sparta::log::Message,
                        typename std::remove_reference<decltype(*this)>::type,
                        &std::remove_reference<decltype(*this)>::type::send_>
                        (node_wptr_.lock().get(), this, *category_, false);
                }
            }

            /*!
             * \brief Detach the tap from a node without destructing. Node can
             * later be reattached to the same or another node using reset.
             * Destination is maintained
             *
             * This method exists to stop observing without having to delete any
             * dynamically allocated taps.
             */
            void detach() {
                if(!node_wptr_.expired()){
                    auto shared = node_wptr_.lock();
                    if(shared){ // Check if reset
                        TreeNodePrivateAttorney::deregisterForNotification<sparta::log::Message,
                                                                           typename std::remove_reference<decltype(*this)>::type,
                                                                           &std::remove_reference<decltype(*this)>::type::send_>(shared.get(),
                                                                                                                                 this, *category_);
                    }
                }else{
                    // Warn that the target node already expired before this tap was destructed
                    // Note that this is one of the only things that cannot be logged because
                    // it can happen at a time when there is all taps that may observe it are being
                    // destroyed. This it not a real issue, but everyone should be aware that it can
                    // happen.
                    //std::cerr << "Warning: Tap of category \"" << *category_ << "\" and destination "
                    //          << dest_->stringize() << " is detaching from an already-expired node"
                    //          << std::endl;
                }

                node_wptr_.reset(); // Clears content
            }

            const std::string* getCategoryID() const {
                return category_;
            }

            const std::string& getCategoryName() const {
                return *category_;
            }

            const Destination* getDestination() const {
                return dest_;
            }

            Destination* getDestination() {
                return dest_;
            }

            /*!
             * \brief Gets the number of messages seen by this tap having the
             * designated category.
             *
             * This is the number of messages forwarded to a destination by
             * this tap.
             */
            uint64_t getNumMessages() const {
                return num_msgs_;
            }

            /*!
             * \brief Gets the node at which this tap is observing.
             * \note This node may have been deleted. To ensure this is safe,
             * use isObservedNodeExpired
             */
            TreeNode* getObservedNode() const {
                return node_;
            }

            /*!
             * \brief Checks if the node at which this tap is observing has been
             * deleted (i.e. its weak reference has expired)
             */
            bool isObservedNodeExpired() const {
                return node_wptr_.expired();
            }

        protected:

            /*!
             * \brief Actually send the notification
             * \param origin Node from which this notification (log message)
             * was generated
             * \param obs_pt_DO_NOT_USE Node at which this tap was installed
             * causing that node or its ancestors to generate this notification
             * (log message). This node may no longer exist at this time because
             * this tap could outlive the node. So do not access this. This will
             * match the node referenced by node_wptr_, so if it is needed,
             * go through node_wptr_ which knows when the node has expired.
             * \note This is a notification callback handler
             * \warning Because Taps can exist while a Tree is being destroyed,
             * this function must not access the observation_point argument
             * because it may not exist
             */
            void send_(const TreeNode& origin,
                       const TreeNode& obs_pt_DO_NOT_USE,
                       const Message& msg) {
                (void)origin;
                (void)obs_pt_DO_NOT_USE;

                sparta_assert(dest_); // Ensure this tap has a valid destination before sending
                dest_->write(msg);
                ++num_msgs_;
            }

        private:

            TreeNode* node_;
            TreeNode::WeakPtr node_wptr_; //!< Weak reference to the TreeNode on which this tap is registered
            const std::string* const category_; //!< From a CategoryManager
            Destination* dest_; //!< Destination object used by this tap.
            uint64_t num_msgs_ = 0; //!< Number of messages seen of the appropriate category (and thus forwardad) by this tap
        };

        /*!
         * \brief Describes a tap
         */
        class TapDescriptor
        {
        public:

            TapDescriptor(const std::string& _loc_pattern,
                          const std::string& _category,
                          const std::string& _destination) :
                loc_pattern_(_loc_pattern),
                category_(_category),
                dest_(_destination)
            { }

            // Allow copies and assignment (needed for vector)
            TapDescriptor(const TapDescriptor&) = default;
            TapDescriptor& operator=(const TapDescriptor&) = default;

            // Pretty print
            std::string stringize() const {
                std::stringstream ss;
                ss << "Tap location_pattern=\"" << loc_pattern_ << "\" (category=\"" << category_
                   << "\") -> file: \"" << dest_ << "\"";
                return ss.str();
            }

            // Increments the usage count of this descriptor
            void incrementUsageCount() const { ++num_times_used_; }

            // Returns the usage count (incremented by incrementUsageCount)
            uint32_t getUsageCount() const { return num_times_used_; }

            // Get the location pattern
            const std::string &getLocation() const { return loc_pattern_; }

            // Get the category
            const std::string &getCategory() const { return category_; }

            // Get the destination file
            const std::string &getDestination() const { return dest_; }

            // Get pattern goodness check
            bool hasBadPattern() const { return has_bad_pattern_; }

            // Set the pattern to be bad
            void setBadPattern(bool bad) const { has_bad_pattern_ = bad; }

        private:

            // Node location string (pattern) on which tap should be placed
            std::string loc_pattern_;

            // Tap category
            std::string category_;

            // Destination file for this tap
            std::string dest_;

            // Number of times this descriptor has been used to
            // construct a tap
            mutable uint32_t num_times_used_ = 0;

            // Is the pattern for this tap known to be false. This is usually
            // detected as the tree is being built when searches fail with an
            // exception
            mutable bool has_bad_pattern_ = false;
        };

        // Convenient typedef
        typedef std::vector<TapDescriptor> TapDescVec;

        /*!
         * \brief Finds all unused taps in the given tap descriptor vector
         * \param taps Vector of taps from which the nubmer of unused taps will be
         * counted
         * \retur Vector of unused tap descriptors
         */
        inline std::vector<const log::TapDescriptor*> getUnusedTaps(const log::TapDescVec& taps) {
            std::vector<const log::TapDescriptor*> unused_taps;
            for(const TapDescriptor& td : taps){
                if(td.getUsageCount() == 0){
                    unused_taps.push_back(&td);
                }
            }
            return unused_taps;
        }

    } // namespace log
} // namespace sparta


