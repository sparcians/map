// <TieredMap> -*- C++ -*-

#pragma once

#include <iostream>
#include <utility>
#include <memory>

#include "sparta/utils/Utils.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"


namespace sparta
{
    /*!
     * \brief N-Tier lookup map for sparse-representation of large memory
     * spaces. This is essentially a M-Tree where the child lookup at each node
     * is a simple offset computed by a rshift and mask.
     * \tparam KeyT Type of key. Should be an unsigned integer or behave like
     * one
     * \tparam ValT Value to associate with key. Must be copy-assignable. Should
     * generally be a pointer as it is stored by value internally.
     * \note This does not behave exactly like a std::map or unordered_map in
     * that the find method returns a pair* instead of an iterator.
     *
     * Represents some number space (e.g. an address space) using a number of
     * tiers dependending on the data being represented.
     *
     * Expected use is for Keys to be memory block indexes (e.g. addr/64)
     * and values to be block objects.
     *
     * \todo Improve iteration to reduce memory use. Instead of maintaining a
     * separate list of pairs, walk the tree itself. However, reasonably fast
     * iteration is needed for checkpointing, so the extra memory use may be
     * somewhat justified
     */
    template <typename KeyT=uint64_t, typename ValT=void*>
    class TieredMap
    {
    public:

        //! \name Types
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief This map contains mappings between a unique set of keys to
         * values. This type represents that mapping for the purposes of
         * iteration and some queries
         */
        typedef std::pair<const KeyT, ValT> pair_t;

        /*!
         * \brief Node in the map tree. Each node contains up to \a node_size
         * children, any of which may be
         *
         * \todo Memory can be further saved by making this sparse itself. Most
         * easily, a start index can be stored in addition to the vector's size.
         * \todo More memory might be saved by making this a simple array
         * instead of a vector. Performance in debug mode would definitely
         * increase because of the removed bounds checking
         */
        typedef std::vector<void*> node_t;

        // Forward declaration for friendship with iterator
        class const_iterator;

        /*!
         * \brief Iterator for walking contents of this map
         *
         * Internally increments and compares a std::vector iterator
         */
        class iterator {
        public:
            typedef typename std::vector<std::unique_ptr<pair_t>>::iterator internal_iter;

            iterator(internal_iter itr) :
                itr_(itr)
            { }

            iterator& operator++() { ++itr_; return *this; }
            iterator operator++(int) { iterator ret(itr_); ++itr_; return ret; }

            iterator& operator=(const iterator& rhp) {
                itr_ = rhp.itr_;
                return *this;
            }

            bool operator==(const iterator& rhp) const {
                return rhp.itr_ == itr_;
            }

            bool operator!=(const iterator& rhp) const {
                return rhp.itr_ != itr_;
            }

            bool operator==(const const_iterator& rhp) const {
                return rhp.itr_ == itr_;
            }

            bool operator!=(const const_iterator& rhp) const {
                return rhp.itr_ != itr_;
            }

            ValT& operator*() { return (*itr_)->second; }
            const ValT& operator*() const { return (*itr_)->second; }

            ValT* operator->() { return &(*itr_)->second; }
            const ValT* operator->() const { return &(*itr_)->second; }

            friend class const_iterator;

        private:
            internal_iter itr_;
        };

        /*!
         * \brief Const for walking contents of this map if const-qualified
         *
         * Internally increments and compares a std::vector iterator
         */
        class const_iterator {
        public:
            typedef typename std::vector<std::unique_ptr<pair_t>>::const_iterator internal_iter;

            const_iterator(internal_iter itr) :
                itr_(itr)
            { }

            const_iterator& operator++() { ++itr_; return *this; }
            const_iterator operator++(int) { const_iterator ret(itr_); ++itr_; return ret; }

            const_iterator& operator=(const const_iterator& rhp) {
                itr_ = rhp.itr_;
                return *this;
            }

            bool operator==(const const_iterator& rhp) const {
                return rhp.itr_ == itr_;
            }

            bool operator!=(const const_iterator& rhp) const {
                return rhp.itr_ != itr_;
            }

            bool operator==(const iterator& rhp) const {
                return rhp.itr_ == itr_;
            }

            bool operator!=(const iterator& rhp) const {
                return rhp.itr_ != itr_;
            }

            const ValT& operator*() const { return (*itr_)->second; }

            const ValT* operator->() const { return &(*itr_)->second; }

            friend class iterator;

        private:
            internal_iter itr_;
        };

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Construction
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Constructor
         * \param tier_size Size of each node
         */
        TieredMap(uint64_t node_size=512) :
            total_size_(0),
            num_nodes_(0), // First node is allocated in ctor
            num_tiers_(0), // First tier is allocated in ctor
            node_size_(node_size),
            tier_shift_(0),
            tier_idx_mask_(~computeMask<uint64_t>(node_size, tier_shift_)) // Computes tier_shift_
        {
            if(false == isPowerOf2(node_size_)){
                throw SpartaException("node_size must be a power of 2, is ")
                    << node_size;
            }

            allocateRoot_();
        }

        //! \brief Destructor
        ~TieredMap() {
            // Free all nodes
            freeNode_(t0_);
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Attributes
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Number of elements allocated in this map
         */
        uint64_t size() const { return pairs_.size(); }

        /*!
         * \brief Number of tiers used by this map to represent the entire space
         * containing the largest key added. Because this map is sparse,
         * has no consistent relationship with memory use.
         */
        uint64_t getNumTiers() const { return num_tiers_; }

        /*!
         * \brief Returns the number of nodes allocated within this map
         */
        uint64_t getNumNodes() const { return num_nodes_; }

        /*!
         * \brief Returns the estimated memory use by this map in bytes
         */
        uint64_t getEstimatedMemory() const {
            return sizeof(TieredMap) // This map
                + (num_nodes_ * sizeof(node_t)) // Each node (container)
                + (num_nodes_ * sizeof(void*) * 0.7) // Content of each node. 0.7 for average utilization
                + (sizeof(pairs_))
                + (pairs_.size() * sizeof(typename decltype(tier_shifts_)::value_type)) // Pair container and content
                + (sizeof(tier_shifts_))
                + (tier_shifts_.size() * sizeof(typename decltype(tier_shifts_)::value_type)); // tier_shift container and content
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Iteration
        //! @{
        ////////////////////////////////////////////////////////////////////////

        const_iterator begin() const { return pairs_.begin(); }
        const_iterator end() const { return pairs_.end(); }
        iterator begin() { return pairs_.begin(); }
        iterator end() { return pairs_.end(); }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Search & Modify
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Clears the map. Frees all mappings and data structures.
         * \post size() will report 0
         */
        void clear() {
            freeNode_(t0_);
            pairs_.clear();
            tier_shifts_.clear();
            num_nodes_ = 0;
            num_tiers_ = 0;

            allocateRoot_(); // Adds first node and tier
        }

        /*!
         * \brief Finds a mapping for key \a k if one exists in this map
         * \param k Key for which a mapping will be located if one exists
         * \return Pair associating key \a k and some \a ValT
         * \note This is not how std::map (or other STL containers) work
         * so this class is not quite a drop-in replacement.
         */
        const pair_t* find(const KeyT& k) const {
            const pair_t* result = tryGet(k);
            if(!result){
                return nullptr;
            }
            return result;
        }

        //! \brief Overload for find
        pair_t* find(const KeyT& k) {
            pair_t* result = tryGet(k);
            if(!result){
                return nullptr;
            }
            return result;
        }

        /*!
         * \brief Finds a value by its key.
         * \return \a ValT reference
         *
         * This can be used to get or set the value
         */
        ValT& operator[](const KeyT& k) {
            pair_t* result = tryGet(k);
            if(result){
                return (result->second);
            }
            return set_(k, 0);
        }

        //! \brief const-qualified index operator
        const ValT& operator[](const KeyT& k) const {
            pair_t* result = tryGet(k);
            if(result){
                return (result->second);
            }
            return set_(k, 0);
        }

        /*!
         * \brief Attempts to get a pair_t associated with the key \a k without
         * modifying the data structure.
         * \param k Key for which a result pair will be located if present.
         * \return nullptr if none found
         * \note At least 1 tier is guaranteed at entry
         */
        pair_t* tryGet(const KeyT& k) {
            void* v = t0_;
            size_t tidx = 0;

            uint64_t vidx = k >> tier_shifts_[tidx];
            if(vidx >= node_size_){
                return nullptr;
            }

            do{
                vidx = k >> tier_shifts_[tidx];
                vidx &= tier_idx_mask_;

                node_t& node = *static_cast<node_t*>(v);
                if(vidx >= node.size()){
                    return nullptr;
                }
                v = node[vidx]; // Points to node in next tier or final pair result
                if(!v){
                    return nullptr;
                }
                ++tidx;
            }while(tidx < num_tiers_);

            return static_cast<pair_t*>(v);
        }

        /*!
         * \brief const-qualified version of tryGet.
         */
        const pair_t* tryGet(const KeyT& k) const {
            void* v = t0_;
            size_t tidx = 0;

            uint64_t vidx = k >> tier_shifts_[tidx];
            if(vidx >= node_size_){
                return nullptr;
            }

            do{
                vidx = k >> tier_shifts_[tidx];
                vidx &= tier_idx_mask_;

                const node_t& node = *static_cast<const node_t*>(v);
                if(vidx >= node.size()){
                    return nullptr;
                }
                v = node[vidx]; // Points to next tier or final pair result
                if(!v){
                    return nullptr;
                }
                ++tidx;
            }while(tidx < num_tiers_);

            return static_cast<const pair_t*>(v);
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:

        /*!
         * \brief Allocates the first node and tier in this map. This is
         * required at construction and resetting of the map.
         * \pre num_nodes_, num_tiers_, and tier_shifts_.size()
         * must be 0.
         */
        void allocateRoot_() {
            sparta_assert(num_nodes_ == 0
                        && num_tiers_ == 0
                        && tier_shifts_.size() == 0,
                        "num_nodes_, num_tiers_, and tier_shifts_.size() must be 0 during allocateRoot_");
            tier_shifts_.push_back(0);
            sparta_assert(tier_shifts_.size() == 1); // Expects exactly 1 shift to start
            t0_ = new node_t; // Allocate a node
            num_nodes_++;
            num_tiers_++;
        }

        /*!
         * \brief Updates the association for key \a k to point to a new value
         * \a n. If no mapping for \a k exists yet, updates the internal data
         * structure to contain it. Otherwisem, the previous association is
         * simply overwritten
         * \note Performs no bounds checking so any key is allowed. and expands
         * the data structures as necessary. Using very large \a k values can
         * use excessive memory
         * \note At least 1 tier is guaranteed at entry
         */
        ValT& set_(const KeyT& k, const ValT n) {
            void* v = t0_;
            size_t tidx = 0;

            uint64_t vidx = k >> tier_shifts_[tidx]; // Value index (within tier)
            if(vidx >= node_size_){
                // Add a new tier because current tier does not represent a
                // large enough space to contain this key
                addTier_();
                return set_(k, n);
            }

            do{
                // Compute index for this tier
                vidx = k >> tier_shifts_[tidx];
                vidx &= tier_idx_mask_;

                node_t& node = *static_cast<node_t*>(v);
                if(vidx >= node.size()){
                    node.resize(vidx+1, nullptr);
                }
                void*& vt = node[vidx]; // Points to next tier or final pair result
                if(!vt){
                    if(tidx < num_tiers_ - 1){
                        //std::cout << "Allocating new vector at tier[0x" << std::hex << vidx << std::dec << "] tidx " << tidx << ", key=0x" << std::hex << k << std::dec << std::endl;
                        // Create a new node at node[vidx]
                        vt = new node_t();
                        ++num_nodes_;
                    }else{
                        //std::cout << "Allocating new pair at tier[0x" << std::hex << vidx << std::dec << "] tidx " << tidx << ", key=0x" << std::hex << k << std::dec << std::endl;
                        // Create a new result pair at node[vidx]
                        pair_t* npair = new pair_t(k, n);
                        pairs_.emplace_back(npair);
                        v = vt = npair;
                        break;
                    }
                }else{
                    if(tidx == num_tiers_ - 1){
                        pair_t* p = static_cast<pair_t*>(vt);
                        p->second = n;
                        v = p;
                        break;
                    }
                }
                v = vt;
                ++tidx;
            }while(tidx < num_tiers_);

            return static_cast<pair_t*>(v)->second;
        }

        /*!
         * \brief Adds a new tier to the top of the tree with the current top
         * becoming its child at index 0. Also adds a new entry to tier_shifts_.
         */
        void addTier_() {
            uint64_t new_shift = tier_shifts_[0] + tier_shift_;
            tier_shifts_.insert(tier_shifts_.begin(), new_shift);
            void* old = t0_;
            t0_ = new node_t;
            t0_->push_back(old); // Old tier at idx=0
            ++num_nodes_;
            ++num_tiers_;
        }

        /*!
         * \brief Recursively deletes a node and its children. It is the
         * parent's responsibility to deal with its deleted children.
         * \param n Node to recursively delete in post-order
         */
        void freeNode_(node_t* n, uint64_t tier=0) {
            sparta_assert(n);
            if(tier < num_tiers_ - 1){
                for(node_t::iterator i = n->begin(); i != n->end(); ++i){
                    node_t* child = static_cast<node_t*>(*i);
                    if(child){
                        freeNode_(child, tier+1);
                    }
                }
            }else{
                for(node_t::iterator i = n->begin(); i != n->end(); ++i){
                    pair_t* p = static_cast<pair_t*>(*i);
                    if(p){
                        // Do not delete p, it is managed by the pairs_ vector.
                    }
                }
            }

            delete n;
        }

        //! Total size of the lookup-space as specified at construction
        const uint64_t total_size_;

        /*!
         * \brief Number of nodes (vectors) allocated for this structure for use
         * in approximating memory use.
         */
        uint64_t num_nodes_;

        /*!
         * \brief Number of lookups to resolve a key to a value. 1 means examine
         * t0 only.
         */
        uint64_t num_tiers_;

        //! Maximum number of entries in a single node
        const uint64_t node_size_;

        //! Shift at final tier (most fine) for computing index from value
        uint64_t tier_shift_ = 0;

        /*!
         * \brief Mask applied after shifting the value for each tier to get an
         * index for that tier
         */
        const uint64_t tier_idx_mask_;

        /*!
         * \brief Total set of allocated pairs in no particular order. Exists to
         * delete mem at destruction
         */
        std::vector<std::unique_ptr<pair_t>> pairs_;

        //! Shift applied at each tier to compute index
        std::vector<uint64_t> tier_shifts_;

        /*!
         * \brief First tier. Points to either a pair_t (from pairs_) or another
         * vector<void*> if looking at a tier other than the highest indexed
         * tier in tier_shifts_
         */
        node_t* t0_;

    }; // class TieredMap

} // namespace sparta

