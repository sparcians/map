// <ResourceFactory> -*- C++ -*-


#ifndef __RESOURCE_FACTORY__
#define __RESOURCE_FACTORY__

#include <iostream>
#include <string>
#include <ostream>
#include <stdexcept>
#include <vector>
#include <memory>
#include <map>

#include "sparta/simulation/Resource.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/utils/Utils.hpp"


namespace sparta
{

    class ResourceTreeNode;

    /*!
     * \brief Factory which can create Resources as well as the
     * Parameter sets that can be modified before actually
     * instantiating the resources themselves. Packages providing
     * resources to the simulator implement this interface.
     *
     * This class must be a factory for exactly 1 type of resource.
     *
     * The paired create/delete methods within this class allow it to be part of
     * a shared-library package interface. This informs the interface design by,
     * for example, prohibiting boost and stl types.
     *
     * These methods exists to ensure that if this factory were created through
     * a shared library, then any memory it allocates will be freed by the same
     * memory manager. This is required to ensure ABI compatibility with shared
     * libraries.
     *
     * Generally, the procedure is:
     * \li params = rfect->createParameters()
     * \li Poplate parameters
     * \l1 Pick a clock for the node
     * \li resource = rfact->createResource(params)
     */
    class ResourceFactoryBase
    {
    public:

        /*!
         * \brief Destructor
         */
        virtual ~ResourceFactoryBase() {}

        /*!
         * \brief Returns the resource type-name for this resource, demangled.
         * \return Resource type as a std::string. This value must be constant
         * for the lifetime of this class.
         */
        virtual std::string getResourceType() const = 0;

        /*!
         * \brief Returns the resource type-name for this resource as in the typeid
         * \return Resource type as a std::string. This value must be constant
         * for the lifetime of this class.
         */
        virtual std::string getResourceTypeRaw() const = 0;

        /*!
         * \brief Creates a new set of parameters associated with the resource
         * that can be created by this factory.
         * \param node TreeNode which which the newly-created parameters will be
         * permanently associated.
         * \return new ParameterSet that must be deallocated by caller via
         * deleteParameters.
         *
         * Each ParameterSet created by this parameter set must contain
         * identical parameters down to  types and default values.
         */
        virtual ParameterSet* createParameters(TreeNode* node) = 0;

        /*!
         * \brief Deletes a ParameterSet created by the createParameters method
         * of this ResourceFactory.
         * \param params ParameterSet to deallocate
         */
        virtual void deleteParameters(ParameterSet* params) = 0;

        /*!
         * \brief Optionally creates a subtree of TreeNodes for this TreeNode
         * by attaching children to this node. These children may be regular
         * TreeNode or ResourceTreeNodes
         *
         * Because this occurs during the tree construction phase, adding
         * child ResourceTreeNodes is valid
         */
        virtual void createSubtree(sparta::ResourceTreeNode* n) = 0;

        /*!
         * \brief Optionally deletes the TreeNodes created by createSubtee (if
         * any).
         *
         * An implementation of ResourceFactoryBase should generally keep a
         * vector of nodes that it allocated and delete them when this method is
         * invoked.
         */
        virtual void deleteSubtree(sparta::ResourceTreeNode* n) = 0;

        /*!
         * \brief Instanitates a new Resource of the type described by this
         * factory.
         * \param node TreeNode with which this resource will be associated. At
         * this point the tree will be finalizing.
         * No new children can be attached to this node at this time.
         *
         * \param params ParameterSet created by createParameters
         * \return Newly created resource that must be deallocated by caller via
         * deleteResource.
         *
         */
        virtual Resource* createResource(TreeNode* node,
                                         const ParameterSet* params) = 0;

        /*!
         * \brief Deletes a resource created by the createResource method of
         * this ResourceFactory
         * \param res Resource to deallocate.
         */
        virtual void deleteResource(Resource* res) = 0;

        /*!
         * \brief Allows contents to be bound together if desired.
         * \pre Tree will be finalized
         *
         * Subclasses can use this to bind ports together if needed, which can
         * also be done in Resource::bind* methods or at the Simulation level
         *
         * This happens before the top-level simulator class has a chance to
         * bind anything
         */
        virtual void bindEarly(TreeNode* node) = 0;

        /*!
         * \brief Allows contents to be bound together if desired
         * \pre Tree will be finalized
         * \pre Top-level simulator class will have had a chance to bind
         *
         * Subclasses can use this to bind ports together if needed, which can
         * also be done in Resource::bind* methods or at the Simulation level
         */
        virtual void bindLate(TreeNode* node) = 0;
    };

    GENERATE_HAS_ATTR(name)
    GENERATE_HAS_ATTR(ParameterSet)

    /*!
     * \brief Templated ResourceFactoryBase implementation which can be used to
     * trivially define Resource Factories.
     * \tparam ResourceT Type of Resource that this factory will instantiate.
     * This type must inherit from sparta::Resource and contain a constructor with
     * the signature:
     * (sparta::TreeNode*, const ParamsT*).
     * \tparam ParamsT ParameterSet class type that this factory will instantiate.
     * This type must inherit from sparta::ParameterSet and provide a constructor
     * with the signature:
     * (sparta::TreeNode*)
     * \note This class is noncopyable.
     *
     * Creating a ResourceFactory a ResourceClass is done by instantiating this
     * class with appropriate template arguments.
     *
     * Example 1 - Explicit Parameter Set types
     * \code
     *
     * class MyResource : sparta::Resource { ... }
     * class MyParamSet : sparta::ParameterSet { ... };
     *
     * typedef ResourceFactory<MyResource, MyParamsSet> MyResourceFactory;
     * \endcode
     *
     * Example 2 - Implicit nested Parameter Set types
     * \code
     *
     * class MyResource : sparta::Resource {
     * public:
     *     // ctor
     *     ...
     *     class ParameterSet : sparta::ParameterSet { ... };
     * };
     *
     * // Looks for MyResource::ParameterSet types.
     * // ParameterSet are required nested classes in this case.
     * typedef ResourceFactory<MyResource> MyResourceFactory;
     * \endcode
     */
    template<typename ResourceT, typename ParamsT=typename ResourceT::ParameterSet>
    class ResourceFactory : public ResourceFactoryBase
    {
    public:

        typedef ResourceT resource_type;
        typedef ParamsT params_type;

        ResourceFactory(const ResourceFactory& rhp) = delete;
        ResourceFactory& operator=(const ResourceFactory& rhp) = delete;

        ResourceFactory() {
            // ParamsT must be a subclass of sparta::ParameterSet.
            // This check will fail if ParamsT is not a subclass of
            // sparta::ParameterSet.
            sparta::ParameterSet* rps = (ParamsT*)0;
            (void) rps;
        };
        ~ResourceFactory() {};

        virtual std::string getResourceType() const override {
            return getResourceType_<ResourceT>();
        }

        virtual std::string getResourceTypeRaw() const override {
            return typeid(ResourceT).name();
        }

        virtual ParameterSet* createParameters(TreeNode* node) override  {
            sparta_assert(node);
            return new ParamsT(node);
        }

        virtual void deleteParameters(ParameterSet* params) override {
            sparta_assert(params, "It is invalid to call deleteParameters with null params");
            delete params;
        }

        // This can easily be overridden to add a subtree
        virtual void createSubtree(sparta::ResourceTreeNode* n) override {
            (void) n;
        }

        virtual void deleteSubtree(sparta::ResourceTreeNode* n) override {
            (void) n;
        }

        /*!
         * \brief Finally instantiates the resource with its set of Parameters
         * \note Cannot be overridden further
         *
         * Params are cast to ParamsT*.  This class would not compile
         * if this cast were impossible.
         */
        Resource* createResource(TreeNode* node,
                                 const ParameterSet* params) override  {
            // Cast Parameters to expected type
            ParamsT const * sps; // specific parameter set
            sps = dynamic_cast<ParamsT const *>(params);
            if(nullptr == sps){ // Specific parameter set must have casted correctly
                throw SpartaException("Failed to cast ParameterSet ")
                    << params << " to type " << typeid(ParamsT).name()
                    << " when constructing resource for node " << node->getLocation();
            }

            return new ResourceT(node, sps); // Construct the new resource
        }

        void deleteResource(Resource* res) override final {
            sparta_assert(res, "It is invalid to call deleteResource with a null resource");
            delete res;
        }

        void bindEarly(TreeNode*) override {;}

        void bindLate(TreeNode*) override {;}

    private:

        template <typename RT>
        typename std::enable_if<has_attr_name<RT>::value, std::string>::type
        getResourceType_() const {
            return ResourceT::name;
        }

        template <typename RT>
        typename std::enable_if<!has_attr_name<RT>::value, std::string>::type
        getResourceType_() const {
            // Generate a friendly verbose error
            static_assert(has_attr_name<RT>::value, "When instantiating a subclass of sparta::ResourceFactory, "
                          "template argument ResourceT is not a complete sparta::Resource subclass: ResourceT type "
                          "template argument to sparta::ResourceFactory must have a public static \"name\" member "
                          "which is a std::string or const char* that contains an alphanumeric name representing "
                          "this Resource.");
            return "";
        }

    };

    /*!
     * \brief Set of published ResourceFactories which can be referenced by name
     */
    class ResourceSet
    {
    public:
        // Typedef for the internal map type
        typedef std::map<std::string, std::unique_ptr<sparta::ResourceFactoryBase>> ResourceFactoryMap;

        /*!
         * \brief Add a resource factory by its template type
         * \tparam ResourceFactoryT Type of factory to add
         * \pre This type of factory must not have been added to this ResourceSet
         * \post An instantiation of ResourceFactoryT will be held inside this
         * ResourceSet and cab be looked up by it's name
         */
        template <typename ResourceFactoryT>
        void addResourceFactory() {
            std::unique_ptr<sparta::ResourceFactoryBase> fact(new ResourceFactoryT());
            sparta_assert(fact != nullptr);
            std::string name = fact->getResourceType();
            auto itr = factories_.find(name);
            if(itr != factories_.end()){
                throw sparta::SpartaException("Cannot reregister ResourceFactory named \"")
                    << name << "\" because there is already a resource registered by that name";
            }
            factories_[name] = std::move(fact);
            max_res_name_len_ = std::max<uint32_t>(max_res_name_len_, name.size());
        }

        //! \brief Returns a ResourceFactory with the given resource type name
        //! \throw SpartaException if resource with given name cannot be found
        //! \note Use renderResources to show all resources
        sparta::ResourceFactoryBase* getResourceFactory(const std::string& name);

        //! \brief Checks for a resource with the given resource type name
        //! \return true if resource with given name is found, false otherwise
        bool hasResource(const std::string& name) const noexcept;

        /*!
         * \brief Returns a string containing all resource names known by this
         * object separated by newlines
         * \param one_per_line Newline between each resource in output. If false,
         * uses commas
         */
        std::string renderResources(bool one_per_line=true);

        /*!
         * \brief Get a constant begin iterator to the ResourceSet
         *
         * Returns an interator to an internal std::pair: first is the
         * name of the factory, second is the pointer to the factory.
         */
        ResourceFactoryMap::const_iterator begin() const {
            return factories_.begin();
        }

        /*!
         * \brief Get a constant end iterator to the ResourceSet
         */
        ResourceFactoryMap::const_iterator end() const {
            return factories_.end();
        }

    private:

        //! map of <resource_name, ResourceFactory*>
        std::map<std::string, std::unique_ptr<sparta::ResourceFactoryBase>> factories_;
        uint32_t max_res_name_len_ = 0; //!< Maximum resource name string length
    };

} // namespace sparta

// __RESOURCE_FACTORY__
#endif
