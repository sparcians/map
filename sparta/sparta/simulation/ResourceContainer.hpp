// <ResourceContainer> -*- C++ -*-

/*!
 * \file ResourceContainer.hpp
 * \brief Object with a name which holds a Resource
 */

#ifndef __RESOURCE_CONTAINER_H__
#define __RESOURCE_CONTAINER_H__

#include "sparta/kernel/PhasedObject.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{
    class Resource;
    class PostRunValidationInfo;
    class Clock;

    /*!
     * \brief PhasedObject which can hold 0 or 1 Resource pointers to an
     * associatedresource. Contains logic for setting and getting the associated
     * Resource.
     *
     * The main motivation for decoupling this from TreeNode is to break a
     * dependency loop with Resource, which attaches and detaches itself with a
     * ResourceContainer at construction and destruction respectively.
     *
     * Has the concept of a locked resource, which means it cannot be unset
     * until the TEARDOWN phase. This prohibits the premature destruction of
     * Resources.
     */
    class ResourceContainer : public PhasedObject
    {
    public:

        /*!
         * \brief Allow resource to invoke setResource_ and unsetResource_
         */
        friend class Resource;

        /*!
         * \brief Consturct with a null, unlocked resource
         */
        ResourceContainer() :
            resource_(nullptr),
            resource_locked_(false),
            num_resource_gets_(0)
        { }

        /*!
         * \brief Copy construction disbled
         */
        ResourceContainer(const ResourceContainer&) = delete;

        /*!
         * \brief Move constructor
         */
        ResourceContainer(ResourceContainer&&) = default;

        /*!
         * \brief Destructor
         * \note Does not free resources
         */
        virtual ~ResourceContainer()
        { }

        /*!
         * \brief Gets the resource contained by this node if any. May only be
         * called after finalization begins or during teardown.
         * \return Resource contained by this ResourceContainer. The same value
         * will always be returned
         * \pre Node is finalized
         *
         * \throw SpartaException if called before finalizing or finalized or
         * tearing down. Throws if node does not have a resource.
         * \see hasResource
         * \see getResourceAs
         *
         * The goal of phase-limited access of a ResourceContainer's resource is
         * to prevent public clients of ResourceContainer from attempting to
         * access a resource before finalization because most resources are not
         * created until finalization. Attempting to get the resource before
         * this point is almost always a misuse.
         *
         * This method throws if the node has no resource because its intended
         * use is by clients who expect certain nodes to have certain resources
         * available. This usage should not require extra null-checks by the
         * developer. So, to keep such code concise, getResource will throw
         * if the node has no resource. Do not call this unless the node is
         * expected to have a resource.
         *
         * The hasResource method is available to determine if the node has a
         * resource before querying it
         */
        Resource* getResource() {
            if(isFinalized() == false
               && isFinalizing() == false
               && isTearingDown() == false){
                throw SpartaException("Cannot call getResource on TreeNode ")
                    << getLocation() << " because it is not finalizing, finalized, or tearing down";
            }
            if(nullptr == resource_){
                SpartaException ex("Cannot call getResource on TreeNode: ");
                ex << getLocation() << " which does not have a resource.";
                if(isFinalized()){
                    ex << " TreeNode is finalized, so it cannot possibly have a resource";
                }else if(isFinalizing()){
                    ex << " TreeNode is finalizing, and might not have created its' resource yet."
                       << " If this TreeNode is expected to have a resource, it just hasn't been finalized yet."
                       << " If this is a DynamicResourceTreeNode, explicitly invoke finalize() on it to"
                       << " immediately create the resource";
                }
                throw ex;
            }
            return getResource_();
        }

        /*!
         * \brief Const variant of getResource
         */
        const Resource* getResource() const {
            if(isFinalized() == false
               && isFinalizing() == false
               && isTearingDown() == false){
                throw SpartaException("Cannot call getResource on TreeNode ")
                    << getLocation() << " because it is not finalizing, finalized, or tearing down";
            }
            if(nullptr == resource_){
                throw SpartaException("Cannot call getResource on TreeNode: ")
                    << getLocation() << " which does not have a resource";
            }
            return getResource_();
        }

        /*!
         * \brief Determines if this node has a resource. This method exists
         * in case the TreeNode is being explored by a tool or interactive UI.
         * Typical TreeNode clients (such as Resources) will assume that there
         * is a resource if they are expecting one.
         * \return true if this node has a resource that will be returned by
         * getResource, false if not.
         * \throw SpartaException if called before finalizing or finalized or
         * tearing down.
         * \see getResource
         */
        bool hasResource() const {
            if(isFinalized() == false
               && isFinalizing() == false
               && isTearingDown() == false){
                throw SpartaException("Cannot call hasResource on TreeNode ")
                    << getLocation() << " because it is not finalizing, finalized, or tearing down";
            }
            return getResource_() != nullptr;
        }

        /*!
         * \brief Gets the resource contained by this node (if any) as the given
         * type
         * \tparam T Type of resource to cast to. Should be sparta::Resource or a
         * subclass
         * \return Pointer to a resource of type T. This class' resource is
         * nullptr before node is finalizing and becomes non-null after
         * finalization (isFinalized).
         * \throw SpartaException if called before finalizing or finalized or
         * tearing down
         */
        template <class T, typename = typename std::enable_if<std::is_pointer<T>::value>::type>
        const T getResourceAs() const {
            if(isFinalized() == false
               && isFinalizing() == false
               && isTearingDown() == false){
                throw SpartaException("Cannot call getResource on TreeNode ")
                    << getLocation() << " because it is not finalizing, finalized, or tearing down";
            }
            if(nullptr == resource_){
                throw SpartaException("Could not get Resource from TreeNode ")
                    << getLocation() << " because it was null. Exepcted type: " << demangle(typeid(T).name());
            }
            const T r = dynamic_cast<T>(resource_);
            if(nullptr == r){
                throw SpartaException("Could not get Resource from TreeNode ")
                    << getLocation() << " because it (" << getResourceTypeName_() << ") could not be cast to type: " << demangle(typeid(T).name());
            }
            return r;
        }

        /*!
         * \brief Overload of getResourceAs for const access with a non-pointer template type
         * \return Pointer to a const resource of type T (T const *).
         * \see getResourceAs
         */
        template <class T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
        const T* getResourceAs() const {
            return getResourceAs<T*>();
        }

        /*!
         * \brief Non-const overload of getResourceAs
         * \return Resource of type T (which will be a pointer).
         * \see getResourceAs
         */
        template <class T, typename = typename std::enable_if<std::is_pointer<T>::value>::type>
        T getResourceAs() {
            if(isFinalized() == false
               && isFinalizing() == false
               && isTearingDown() == false){
                throw SpartaException("Cannot call getResource on TreeNode ")
                    << getLocation() << " because it is not finalizing, finalized, or tearing down";
            }
            if(nullptr == resource_){
                throw SpartaException("Could not get Resource from ResourceTreeNode \"")
                    << getLocation() << "\" because it was null. Exepcted type: " << demangle(typeid(T).name());
            }
            T r = dynamic_cast<T>(resource_);
            if(nullptr == r){
                throw SpartaException("Could not get Resource from ResourceTreeNode \"")
                    << getLocation() << "\" because it (" << getResourceTypeName_() << ") could not be cast to type: " << demangle(typeid(T).name());
            }
            return r;
        }

        /*!
         * \brief Non-const overload of getResourceAs
         * \return Pointer to a const resource of type T (T*)
         * \see getResourceAs
         */
        template <class T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
        T* getResourceAs() {
            return getResourceAs<T*>();
        }

        /*!
         * \brief Gets the typename of the resource that this node will eventually contain
         * \return non-empty string.
         *
         * Returns a demangled typeid of the internal resource type
         */
        virtual std::string getResourceType() const;

        /*!
         * \brief Gets the typename of the resource that this node will eventually contain
         * \return non-empty string.
         *
         * Returns a raw typeid of the internal resource type
         */
        virtual std::string getResourceTypeRaw() const;

        /*!
         * \brief Gets the clock associated with this ResourceContainer, if any
         * \return The clock associated with this ResourceContainer if there is
         * one. Otherwise returns nullptr.
         * \warning Do not call this in performance-critical code. The returned
         * clock should be cached
         */
        virtual const Clock* getClock() = 0;

    protected:

        /*!
         * \brief Gets the rtti type name (demangled) of the resource type held by this container.
         * If there is no resource held, returns empty string.
         */
        std::string getResourceTypeName_() const;

        /*!
         * \brief Allows subclasses to assign the resource associated with this
         * node.
         * \param r Resource pointer to store internally. Must not be nullptr.
         * Caller owns this resource. This class method just stores a reference
         * to be accessed through getResource_
         * \pre Current resource pointer (getResource) must be nullptr.
         * \post Current resource pointer will not be nullptr
         * \note Expected to be called from a sparta::Resource being constructed
         */
        void setResource_(Resource* r) {
            if(resource_locked_){
                THROW_IF_NOT_UNWINDING(SpartaException,
                                       "Resource pointer on " << getLocation()
                                        << " has been locked. It cannot be set");
            }
            if(resource_ != nullptr){
                THROW_IF_NOT_UNWINDING(SpartaException,
                                       "Resource pointer on " << getLocation()
                                       << " has already been set. It cannot be replaced. ");
            }
            if(r == nullptr){
                THROW_IF_NOT_UNWINDING(SpartaException,
                                       "Resource pointer on " << getLocation()
                                       << " cannot be assigned to nullptr. ");
            }

            resource_ = r;
        }

        /*!
         * \brief Allows a resource to unset the resource set with setResource_
         * \note This should happen during destruction
         * \pre This node has either not locked its resource or this node is in
         * the TEARDOWN phase.
         * \param r Resource pointer to store internally. Must not be nullptr.
         * \post isResourceLocked_ will return true
         * \note Expected to be called from a sparta::Resource being destructed
         */
        void unsetResource_() {
            if(resource_locked_ && false == isTearingDown()){
                THROW_IF_NOT_UNWINDING(SpartaException,
                                       "Resource pointer on " << getLocation()
                                       << " has been locked. It cannot be unset until teardown");
            }
            resource_ = nullptr;
        }

        /*!
         * \brief Allows subclasses to assign the resource associated with this
         * node.
         * \note There is no harm in calling this more than once
         * \note Current resource pointer (getResource) should ne non=null,
         * but this is not required
         * \post resource can no longer be cleared except during teardown phase
         * \see unsetResource_
         */
        void lockResource_() {
            resource_locked_ = true;
        }

        /*!
         * \brief Returns the currently held resource of this node (if any).
         * This method can be called at any time.
         *
         * Note that this is different from the public getResource in that it
         * can be called at any time without the possibility of throwing an
         * exception.
         */
        Resource* getResource_() noexcept {
            ++num_resource_gets_;
            return resource_;
        }

        /*!
         * \brief Const variant of getResource_
         */
        const Resource* getResource_() const noexcept {
            ++num_resource_gets_;
            return resource_;
        }

    private:

        /*!
         * \brief Resource associated with this node. Not owned by TreeNode
         */
        Resource* resource_;

        /*!
         * \brief Is the resource_ member allowed to be changed
         */
        bool resource_locked_;


        //! \name Internal class mis-use metrics
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Number of times a the resource has been requested
         * \todo Use this
         */
        mutable uint32_t num_resource_gets_;

        ////////////////////////////////////////////////////////////////////////
        //! @}
    };

} // namespace sparta

// __RESOURCE_CONTAINER_H__
#endif
