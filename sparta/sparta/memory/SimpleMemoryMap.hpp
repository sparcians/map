
/**
 * \file   SimpleMemoryMap.hpp
 * \brief  File that contains SimpleMemoryMap
 */

#ifndef __SIMPLE_MEMORY_MAP_H__
#define __SIMPLE_MEMORY_MAP_H__

#include "sparta/memory/MemoryExceptions.hpp"
#include "sparta/memory/AddressTypes.hpp"
#include "sparta/memory/BlockingMemoryIF.hpp"

namespace sparta
{
    namespace memory
    {
        /*!
         * \brief Memory mapping object which maps addresses onto block-aligned
         * destinations, each of which is a BlockingMemoryIF object. This method
         * does not actually support memory accesses, only mapping and querying.
         *
         * Mapping is performed within this map and is invisible to clients
         * of this class. Internal mapping is not considered a translation and
         * there is no TranslationIF associated with the internals mapping of
         * this class.
         *
         * Mapped ranges can be added only (not removed), cannot overlap,
         * can, however, map to overlapping ranges on the same destination
         * memory.
         *
         * All mappings are Affine and contiguous. For mapping multiple regions
         * to the same object or mapping one range to discontinuous or
         * overlapping ranges in a destination memory object, use separate
         * mappings.
         *
         * Implemented as a red-black tree to balance the tree and make lookups
         * more consistently log(n).
         *
         * Example
         * \code
         * using sparta::memory;
         * // SimpleMemoryMap* smm;
         * // BlockingMemoryIF* mem1;
         * // BlockingMemoryIF* mem2;
         * smm->addMapping(0x200,0x240,mem1,0);
         * smm->addMapping(0x240,0x280,mem2,0);
         *
         * BlockingMemoryIF* bmi = smm->findInterface(paddr);
         * assert(bmi == mem1);
         * cout << "Addr 0x" << paddr << std::dec << " went to: " << bmi->getDescription();
         * \endcode
         */
        class SimpleMemoryMap
        {
        public:

            /*!
             * \brief Represents a mapping between an input address and output
             * address for use in a destination BlockingMemoryIF
             */
            struct Mapping
            {
                /*!
                 * \brief construct a mapping using the same values received
                 * from SimpleMemoryMap::addMapping
                 */
                Mapping(addr_t _start, addr_t _end, BlockingMemoryIF* _memif, addr_t _dest_off) :
                    start(_start),
                    end(_end),
                    dest_off(_dest_off),
                    memif(_memif)
                {
                    sparta_assert(memif != nullptr);
                }

                /*!
                 * \brief Beginning of the mapping input range (inclusive)
                 *
                 * Value to subtract from incoming address to remap. This
                 * will always be <= the incoming address because it is the
                 * start of the mapping in the input address space.
                 *
                 * Add this number to memif first because result will always be
                 * >= 0
                 */
                const addr_t start;

                /*!
                 * \brief End of the mapping input address range (exclusive)
                 */
                const addr_t end;

                /*!
                 * \brief Offset into destination memory interface. This is an
                 * offset from 0 which is received when the input address equals
                 * \a start.
                 *
                 * Value to add to incoming address to remap. This exists
                 * in addition to subtract so that mappings equal to + or - the
                 * entire address space can be done.
                 */
                const addr_t dest_off;

                /*!
                 * \brief Memory interface mapped to (after add/sub are applied
                 * to address)
                 */
                BlockingMemoryIF* const memif;

                /*!
                 * \brief Maps an input address to the address-space for the
                 * destination memory interface.
                 */
                addr_t mapAddress(addr_t input) const noexcept {
                    return (input - start) + dest_off;
                }

                /*!
                 * \brief Returns true if \a a is in the range [start,end)
                 * \param a Address to test in the range
                 */
                bool contains(addr_t a) const noexcept {
                    return a >= start && a < end;
                }

                /*!
                 * \brief Returns true if any part of the range [a,b) is shared
                 * with range [start,end)
                 * \param a Start (inclusive) of test range
                 * \param e End (exclusive) of test range
                 */
                bool overlaps(addr_t a, addr_t b) const noexcept {
                    return (a >= start && a < end) || (b > start && b <= end) || (a < start && b > end);
                }
            };

            //! \name Construction
            //! @{
            ////////////////////////////////////////////////////////////////////////

            SimpleMemoryMap() = delete;

            /*!
             * \brief Construct a SimpleMemoryMap
             * \param block_size Size of blocks in mapping. Must match or be
             * smaller than all BlockingMemoryIF instances to which this object
             * will maps. Must be a power of 2 and greater than 0
             */
            SimpleMemoryMap(addr_t block_size) :
                block_size_(block_size),
                bintree_(nullptr),
                num_mappings_(0)
            {
                sparta_assert(block_size > 0, "block size must be greater than 0");
                block_idx_rshift_ = (addr_t)log2(block_size);
                block_offset_mask_ = (1 << block_idx_rshift_) - 1;
            }

            /*!
             * \brief Destructor
             */
            virtual ~SimpleMemoryMap() {
                delete bintree_;
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Mapping Interface
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Create a mapping from addresses entering this object to a
             * destination memory interface.
             * \param start Start address of mapping region. Must be
             * block-aligned
             * \param end End address (exclusive) of mapping region. Must be
             * block-aligned. Must be > \a start (mapping must be 1 or more
             * bytes). The range defined by [start,end) cannot overlap any other
             * mapping range already added to this object. Edges may be shared
             * though (e.g. range 1 end can equal range 2 start).
             * \param memif Memory interface to which accesses in the range
             * defined by [start,end) will be forwarded with the new address of:
             * \a address - \a start + \a dest_start.
             * The block size (BlockingMemoryIF::getBlockSize) of this interface
             * must be exactly the size of this SimpleMemoryMap block size.
             * Requiring the block size to be equal means that no legal accesses
             * can be made through this interface which span blocks in
             * destination memory interfaces. This allows this class to avoid
             * testing for mapping-spanning accesses because the destination
             * interfaces are expected to reject them. If a destination
             * BlockingMemoryIF were to violate this assumption, an access could
             * be made through this map which spanned blocks and thus spanned
             * mapping ranges - this would be undesirable behavior.
             * \param dest_start Added address offset at destination. Must be a
             * multiple of block_size - effectively limits granulariry of any
             * mapping to whole-block-to-whole-block.
             * If dest_start were 0, accesses with an address equal to \a start
             * to be forwarded to \a memif with an address of 0. Accesses with
             * address = \a start+4 would be forwarded with an address of 4.
             * \a dest_start allows this to be adjusted by adding an offset
             * to the destination address.
             *
             * \note Validates memif to ensure that the entire range specified
             * by [ \a start, \a end ) can actually be mapped to accessible
             * values within
             * \note Mappings are allowed to span blocks, but endpoints must be
             * block-aligned. By requiring this alignment, and forwarding
             * accesses onto BlockingMemoryIFs which require non-block-spanning
             * accesses, it is guaranteed that accesses will not span mappings
             * in this map. This is also why dest_start must be a block multiple.
             * \note Accesses will be allowed to span blocks within this
             * interface. It is the responsibility of the recieving \a memif
             * to check accesses for block-spanning.
             * \note The minimum address that will be accessed in the
             * destination \a memif is \a dest_start.
             * The maximum address that will be accessed in the
             * destination \a memif is \a dest_start + (end-start) - 1
             * \a memif \b must support this range of accesses
             */
            void addMapping(addr_t start,
                            addr_t end,
                            BlockingMemoryIF* memif,
                            addr_t dest_start) {
                sparta_assert(memif);
                sparta_assert(memif->getBlockSize() == block_size_);
                sparta_assert(start < end);
                sparta_assert((start & block_offset_mask_) == 0);
                sparta_assert((end & block_offset_mask_) == 0);
                sparta_assert((dest_start & block_offset_mask_) == 0);

                const addr_t required_range = (end-start) + dest_start;
                if(memif->getRange() < required_range){
                    throw SpartaException("Total range of destination memory interface is ")
                        << "too small to contain all mappings from SimpleMemoryMap mapping "
                        << "[" << start << ", " << end << ") -> " << memif
                        << " with dest_start" << dest_start << ". Mapped input range size "
                        << "exceeds memory interface (with range " << memif->getRange()
                        << " by " << required_range - memif->getRange();
                }

                // Verify that the stand and end addresses do not contain do not
                // overlap any mappings. This information could be extracted
                // from the tree, but it is simple to explicitly test here to
                // ensure that no tree changes need to be rolled back. There is no
                // performance concern for insertion
                for(auto& m : mappings_) {
                    if(m->overlaps(start,end)){
                        throw SpartaException("Cannot add another mapping [")
                            << start << ", " << end
                            << ") which overlaps another. "
                            << " mapping occupying [" << m->start
                            << ", " << m->end << ")";
                    }
                }

                addr_t min = 0;
                addr_t max = ~(addr_t)0;
                BinTreeNode* n = bintree_;

                // Insert left endpoint
                if(!n){
                    // No root yet
                    min = start;
                    n = bintree_ = new BinTreeNode(nullptr, start);
                    RBTreeFixup_(n);
                    // No fixing should actually have taken place
                }else{
                    while(1){
                        if(n->dest != nullptr){
                            Mapping* m = n->dest.get();
                            // Children nodes on a side will always exist if start is outside this mapping on that side
                            if(start < m->start){
                                sparta_assert(n->l);
                                n = n->l;
                            }else if(start >= m->end){
                                sparta_assert(n->r);
                                n = n->r;
                            }else{
                                throw SpartaException("Cannot add another mapping [")
                                    << start << ", " << end
                                    << ") in a range previously occupied by another. "
                                    << "Destination found occupying [" << m->start
                                    << ", " << m->end << "). Found when placing left endpoint";
                            }
                        }else if(n->separator == start){
                            // Insert right endpoint immediately
                            min = start;
                            //std::cout << "\nAlready had START sepatator @ " << start << std::endl;
                            break;
                        }else if(n->separator < start){
                            min = n->separator;
                            if(n->r){
                                sparta_assert(n->separator >= min);
                                n = n->r;
                            }else{
                                n->r = new BinTreeNode(n, start);
                                n = n->r;
                                RBTreeFixup_(n);
                                // If n was relocated up in the tree, the right-endpoint
                                // insertion loop will find it.
                                break;
                            }
                        }else{
                            max = n->separator;
                            if(n->l){
                                sparta_assert(n->separator <= max);
                                n = n->l;
                            }else{
                                n->l = new BinTreeNode(n, start);
                                n = n->l;
                                RBTreeFixup_(n);
                                // If n was relocated up in the tree, the right-endpoint
                                // insertion loop will find it.
                                break;
                            }
                        }
                    } // while(1)
                } // if(!n) { } else {

                // Insert right endpoint
                while(1){
                    if(n->dest != nullptr){
                        Mapping* m = n->dest.get();
                        // Children nodes on a side will always exist if start is outside this mapping on that side
                        if(end <= m->start){
                            assert(n->l);
                            n = n->l;
                        }else if(end > m->end){
                            sparta_assert(n->r);
                            n = n->r;
                        }else{
                            throw SpartaException("Cannot add another mapping [")
                                << std::hex << start << ", " << end
                                << ") in a range previously occupied by another. "
                                << "Destination found occupying [" << m->start
                                << ", " << m->end << "). Found when placing right endpoint";
                        }
                    }else if(n->separator == end){
                        // Insert destination now
                        max = end;
                        //std::cout << "\nAlready had END sepatator @ " << end << std::endl;
                        break;
                    }else if(n->separator < start){
                        throw SpartaException("Node separator ")
                            << n->separator
                            << " encountered when placing right endpoint cannot be less than the start of the range";
                    }else if(n->separator == start){
                        min = n->separator;
                        if(n->r){
                            sparta_assert(n->separator <= max);
                            n = n->r;
                        }else{
                            // Is new endpoint required or is this node already constrained by ancestors
                            if(max != end){
                                n->r = new BinTreeNode(n, end);
                                n = n->r;
                                RBTreeFixup_(n);
                            }
                            break;
                        }
                    }else if(n->separator < end){
                        throw SpartaException("Cannot add another mapping [")
                            << std::hex << start << ", " << end
                            << ") in a range previously occupied by another. "
                            << "Separator at " << n->separator
                            << " found occupying [" << min << ", " << max << ")";
                    }else{
                        // n->sepatator > end
                        max = n->separator;
                        if(n->l){
                            sparta_assert(n->separator <= max);
                            n = n->l;
                        }else{
                            // Is new endpoint required or is this node already constrained by ancestors
                            if(max != end){
                                n->l = new BinTreeNode(n, end);
                                n = n->l;
                                RBTreeFixup_(n);
                            }
                            break;
                        }
                    }
                } // while(1)

                // Place final destination node
                // This will always be a child of a separator node. We cannot
                // have two destination nodes in a parent-child relationship
                // because separator nodes always define their edges.
                sparta_assert(n);
                sparta_assert(n->separator != ~0ULL);
                std::unique_ptr<Mapping> m(new Mapping(start, end, memif, dest_start));
                if(end <= n->separator){
                    if(n->l){
                        // Fixup moved n around and attached a left child where
                        // the destination node would be. Find the start separator node
                        n = findSeparatorNode_(bintree_, start);
                        sparta_assert(!n->r); // Ensure this has no children to the left that we might be about to replace
                        n->r = new BinTreeNode(n, std::move(m)); // Takes ownership
                        n = n->r;
                    }else{
                        n->l = new BinTreeNode(n, std::move(m)); // Takes ownership
                        n = n->l;
                    }
                }else if(start >= n->separator){
                    if(n->r){
                        // Fixup moved n around and attached a right child where
                        // the destination node would be. Find the end separator node
                        n = findSeparatorNode_(bintree_, end);
                        sparta_assert(!n->l); // Ensure this has no children to the right that we might be about to replace
                        n->l = new BinTreeNode(n, std::move(m)); // Takes ownership
                        n = n->l;
                    }else{
                        n->r = new BinTreeNode(n, std::move(m)); // Takes ownership
                        n = n->r;
                    }
                }else{
                    throw SpartaException("Error placing destination mapping node [")
                        << start << ", " << end << "). Range somehow spanned a separator at " << n->separator << ". This should not have occurred";
                }
                RBTreeFixup_(n);

                num_mappings_ += 1;
                mappings_.push_back(n->dest.get());
            }

            /*!
             * \brief Dumps the tree to an ostream like a directory listing
             * \param o ostream to which tree will be printed
             */
            void dumpTree(std::ostream& o) const {
                if(bintree_){
                    bintree_->recursDump(o);
                }
            }

            /*!
             * \brief Dumps a list of mappings to an ostream with a newline
             * after each mapping entry.
             * \param o ostream to which mappings will be printed
             */
            void dumpMappings(std::ostream& o) const {
                uint32_t desc_len = 1;
                uint32_t start_len = 1;
                uint32_t end_len = 1;
                for(const Mapping* m : mappings_){
                    desc_len = std::max<uint32_t>(m->memif->getDescription().size(), desc_len);
                    std::stringstream tmp;
                    tmp << std::showbase << std::hex << m->start;
                    start_len = std::max<uint32_t>(tmp.str().size(), start_len);
                    tmp.str("");
                    tmp << std::showbase << std::hex << m->end;
                    end_len = std::max<uint32_t>(tmp.str().size(), end_len);
                }

                // Sort content
                std::vector<const Mapping*> sorted = mappings_;

                std::sort(sorted.begin(), sorted.end(),
                          [](const Mapping* m1, const Mapping* m2){return m1->start < m2->start;});

                auto f = o.flags();
                for(const Mapping* m : sorted){
                    o << "map: [" << std::hex << std::showbase << std::setw(start_len)
                      << m->start << ", " << std::setw(end_len) << m->end
                      << std::noshowbase << ") -> \"" << std::setw(desc_len)
                      << m->memif->getDescription() << "\" +0x" << m->dest_off<< '\n';
                }
                o.flags(f);
            }

            /*!
             * \brief Returns the destination memory interface associated with a
             * mapping containing an address.
             * \return Memory interface for the mapping containing this address.
             * If not found, returns nullptr.
             */
            BlockingMemoryIF* findInterface(addr_t addr) {
                Mapping* m = findMapping(addr);
                if(!m){
                    return nullptr;
                }
                return m->memif;
            }

            /*!
             * \brief Finds the Mapping object associated with an address
             * \return Mapping associated with an address if address is
             * contained in a mapping. If not found, returns nullptr.
             */
            Mapping* findMapping(addr_t addr) {
                // Navigate the bintree to find the addr (if contained)
                BinTreeNode* n = bintree_;
                Mapping* m = nullptr;

                // Search tree for first mapping
                while(n){
                    if(n->dest){
                        m = n->dest.get();
                        if(addr < m->start){
                            n = n->l;
                        }else if(addr >= m->end){
                            n = n->r;
                        }else{
                            return m;
                        }
                    }else if(addr >= n->separator){
                        n = n->r;
                    }else{
                        n = n->l;
                    }
                }
                if(!n){
                    return nullptr;
                }

                return m;
            }

            /*!
             * const-qualified version of findMapping
             */
            const Mapping* findMapping(addr_t addr) const {
                // Navigate the bintree to find the addr (if contained)
                const BinTreeNode* n = bintree_;
                const Mapping* m = nullptr;

                // Search tree for first mapping
                while(n){
                    if(n->dest){
                        m = n->dest.get();
                        if(addr < m->start){
                            n = n->l;
                        }else if(addr >= m->end){
                            n = n->r;
                        }else{
                            return m;
                        }
                    }else if(addr >= n->separator){
                        n = n->r;
                    }else{
                        n = n->l;
                    }
                }
                if(!n){
                    return nullptr;
                }
                return m;
            }

            /*!
             * \brief Determines if a mapping is valid or not.
             * \throw MemoryAccessError if mapping is not valid
             *
             * Note that normal read/write paths may not perform a check this
             * careful for performance reasons
             */
            void verifyHasMapping(addr_t addr, addr_t size) const {
                const addr_t end = addr+size;

                const BinTreeNode* n = bintree_;
                const Mapping* m = nullptr;

                while(n){
                    if(n->dest){
                        m = n->dest.get();
                        if(addr < m->start){
                            n = n->l;
                        }else if(addr >= m->end){
                            n = n->r;
                        }else if(end <= m->end){
                            return; // Ok. Reached a destination containing both addr and end
                        }else{
                            throw MemoryAccessError(addr, size, "any", "This access spans more than one mapping");
                        }
                    }else if(addr >= n->separator){
                        n = n->r;
                    }else{
                        n = n->l;
                    }
                }
                throw MemoryAccessError(addr, size, "any", "No single mapping found for this address/size");
            }

            /*!
             * \brief Maps an input address to a destination interface and
             * address within that destination interface.
             * \param add Input address to map
             * \return Pair containing destination BlockingMemoryIF and an
             * address within that interface
             * If no mapping is found for the given address, the memory
             * interface (first) will be nullptr
             *
             * \note As long as accesses to the resulting destination are
             * constrained to blocks, the result can be cached for future
             * accesses of any size provided that they are within the same
             * block as the resulting address
             *
             * Example:
             * \code
             * // uint8_t data[12];
             * auto mapping = map.mapAddress(0xf3);
             * if(mapping.first != nullptr){
             *   mapping.first.write(mapping.second, 12, data);
             * }
             * \endcode
             */
            std::pair<const BlockingMemoryIF*, addr_t> mapAddress(addr_t addr) const noexcept {
                const Mapping* m = findMapping(addr);
                if(!m){
                    return {nullptr, 0};
                }
                return {m->memif, m->mapAddress(addr)};
            }

            /*!
             * \brief Returns the number of mappings successfully added to this
             * map
             */
            uint32_t getNumMappings() const {
                return num_mappings_;
            }

            /*!
             * \brief Returns the vector of current mappings in the order added.
             */
            const std::vector<const Mapping*>& getMappings() const {
                return mappings_;
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Attributes
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Returns the block size of memory represented by this
             * interface. Read and write accesses must not span block
             * boundaries (where addr % block_size == 0).
             */
            addr_t getBlockSize() const { return block_size_; }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            /*!
             * \brief Render description of this SimpleMemoryMap as a string
             */
            std::string stringize(bool pretty=false) const {
                (void) pretty;
                std::stringstream ss;
                ss << "<SimpleMemoryMap " << num_mappings_ << " mappings>";
                return ss.str();
            }

        private:

            /*!
             * \brief Node in binary tree for performing mapping lookups
             *
             * For performance, avoiding initialization of unused vars.
             * Only if dest==nullptr will l,r, and separator be relevant.
             */
            struct BinTreeNode
            {
                //! \brief No default construction
                BinTreeNode() = delete;

                //! \brief No copy construction
                BinTreeNode(const BinTreeNode&) = delete;

                //! \brief No copy assignment
                BinTreeNode& operator=(const BinTreeNode&) = delete;

                /*!
                 * \brief Constructor for seprator node
                 * \param p Parent Node
                 * \param sep Separator. Anything >= sep goes to the right child
                 */
                BinTreeNode(BinTreeNode* p, addr_t sep) :
                    separator(sep),
                    parent(p),
                    l(nullptr),//new BinTreeNode(this)), // Leaf
                    r(nullptr),//new BinTreeNode(this)), // Leaf
                    dest(nullptr),
                    red(true)
                { }

                /*!
                 * \brief Constructor for destination node
                 * \param p Parent Node
                 * \param d Mapping destination for this (leaf) node
                 * \warnings l,r, and separator must never be used
                 */
                BinTreeNode(BinTreeNode* p, Mapping* d) :
                    separator(~(addr_t)0),
                    parent(p),
                    l(nullptr),//new BinTreeNode(this)), // Leaf
                    r(nullptr),//new BinTreeNode(this)), // Leaf
                    dest(d),
                    red(true)
                { }

                /*!
                 * \brief Constructor for destination node with rvalue
                 * unique_ptr
                 * \param p Parent node
                 * \param d Mapping destination for this (leaf) node as an
                 * rvalue-reference to be moved
                 * \warnings l,r, and separator must never be used
                 */
                BinTreeNode(BinTreeNode* p, std::unique_ptr<Mapping>&& d) :
                    separator(~(addr_t)0),
                    parent(p),
                    l(nullptr),//new BinTreeNode(this)), // Leaf
                    r(nullptr),//new BinTreeNode(this)), // Leaf
                    dest(std::move(d)),
                    red(true)
                { }

                /*!
                 * \brief Node destructor
                 */
                ~BinTreeNode() {
                    delete l;
                    delete r;
                }

                /*!
                 * \brief Returns grandparent (nullptr if no parent or
                 * grandparent)
                 */
                BinTreeNode* grandparent() {
                    if(parent){
                        return parent->parent;
                    }
                    return nullptr;
                }

                /*!
                 * \brief Returns sibling of parent node (nullptr if parent or
                 * grandparent)
                 */
                BinTreeNode* uncle() {
                    BinTreeNode *g = grandparent();
                    if(!g){
                        return nullptr;
                    }
                    if(parent == g->l){
                        return g->r;
                    }
                    return g->l;
                }

                /*!
                 * \brief Write the content of the tree to the given ostream
                 * \a o with each l and r child indented.
                 * \param o Ostream to which tree will be written
                 * \param depth Current depth (for determining indentation
                 * amount)
                 */
                void recursDump(std::ostream& o, int depth=0) const {
                    auto f = o.flags();
                    if(red){
                        o << "(R) ";
                    }else{
                        o << "(B) ";
                    }
                    if(dest){
                        o << "map: [0x" << std::hex << dest->start << ", 0x" << dest->end
                          << ") -> memif:" << dest->memif << " \"" << dest->memif->getDescription() << "\" dest_offset=+0x" << dest->dest_off<< '\n';
                    }else{
                        o << "sep: " << std::hex << "0x" << separator << '\n';
                    }

                    o.flags(f);

                    for(int i=0; i<depth; ++i){
                        o << "  ";
                    }
                    o << "l: ";
                    if(l){
                        l->recursDump(o, depth+1);
                    }else{
                        o << "-\n";
                    }

                    for(int i=0; i<depth; ++i){
                        o << "  ";
                    }
                    o << "r: ";
                    if(r){
                        r->recursDump(o, depth+1);
                    }else{
                        o << "-\n";
                    }
                }

                const addr_t separator; //!< Separator. Used if dest==nullptr. If addr >= separator, relevant child is r; otherwise l
                BinTreeNode* parent;    //!< Parent node (pointer not owned)
                BinTreeNode* l;         //!< Next node if address is < separator. nullptr if no mappings to right
                BinTreeNode* r;         //!< Next node if adress is >= separator. nullptr if no mappings to left
                const std::unique_ptr<Mapping> dest; //!< Destination reached at this node. No further traversal needed if dest!=nullptr
                bool red;               //!< Color of the node for RB-tree. If false, black
            };

            /*!
             * \brief Perform red-black tree insertion fixup to balance the tree
             * \param n Node to fix then ascend the tree and fix.
             */
            void RBTreeFixup_(BinTreeNode* n){
                sparta_assert(n);

                n->red = true; // Inserted node colored red
                // Ascend tree and fix RB-Tree rule violations
                while((bintree_ != n) && (n->parent->red)){
                    if(n->parent == n->parent->parent->l){
                        BinTreeNode* u = n->parent->parent->r;
                        if(u && u->red){
                            // RB-Tree case 1 - Recolor
                            n->parent->red = false;
                            u->red = false;
                            n->parent->parent->red = true;
                            n = n->parent->parent;
                        }else{
                            // Uncle is black (RB-Tree null-leafs would be black)
                            if(n == n->parent->r){
                                // RB-Tree case 2 - move n up and rotate left
                                n = n->parent;
                                rotateLeft_(n);
                            }
                            // RB-Tree case 3 - Recolor
                            n->parent->red = false;
                            n->parent->parent->red = true;
                            rotateRight_(n->parent->parent);
                        }
                    }else{
                        // Identical to logic in previous if statement with l/r reversed
                        BinTreeNode* u = n->parent->parent->l;
                        if(u && u->red){
                            n->parent->red = false;
                            u->red = false;
                            n->parent->parent->red = true;
                            n = n->parent->parent;
                        }else{
                            if(n == n->parent->l){
                                n = n->parent;
                                rotateRight_(n);
                            }
                            n->parent->red = false;
                            n->parent->parent->red = true;
                            rotateLeft_(n->parent->parent);
                        }
                    }
                }

                bintree_->red = false; // RB-Tree root is always blackx
            }

            /*!
             * \brief Find the node with a separator value equal to the \a addr
             * parameter
             * \param root Start of binary search. Typically this is the root.
             * \param addr address of the search
             * \return Node having separator equal to \a addr
             *
             * This is used after an RBTreeFixup_ after placing an range
             * endpoint separator node when the newly-placed node is found to be
             * relocated and gains an unexpected left child where the next node
             * would be. If this happens, it will always be the case that the
             * other end-point separator node will have a free child pointer
             * needed to insert the mapping. This method will find that
             * separator
             */
            BinTreeNode* findSeparatorNode_(BinTreeNode* root, addr_t addr) {
                BinTreeNode* n = root;
                while(1){
                    if(n->dest){
                        const Mapping* m = n->dest.get();
                        if(addr < m->start){
                            n = n->l;
                        }else if(addr >= m->end){
                            n = n->r;
                        }else{
                            throw SpartaException("Looking for a separator node at addr=")
                                << addr << " ended up within a mapping node";
                        }
                    }else if(addr == n->separator){
                        return n;
                    }else if(addr > n->separator){
                        n = n->r;
                    }else{
                        n = n->l;
                    }
                }
                return n;
            }

            /*!
             * \brief Rotates a subtree with root node \a n to the left
             */
            void rotateLeft_(BinTreeNode* n){
                sparta_assert(n);
                sparta_assert(n->r);
                BinTreeNode* pivot = n->r;
                n->r = pivot->l;
                if(n->r){
                    n->r->parent = n;
                }
                pivot->parent = n->parent;

                if(n->parent == nullptr){
                    bintree_ = pivot;
                }else{
                    if(n == n->parent->l){
                        n->parent->l = pivot;
                    }else{
                        n->parent->r = pivot;
                    }
                }
                pivot->l = n;
                n->parent = pivot;
            }

            /*!
             * \brief Rotates a subtree with root node \a n to the right
             */
            void rotateRight_(BinTreeNode* n){
                sparta_assert(n);
                sparta_assert(n->l);
                BinTreeNode* pivot = n->l;
                n->l = pivot->r;
                if(n->l){
                    n->l->parent = n;
                }
                pivot->parent = n->parent;

                if(n->parent == nullptr){
                    bintree_ = pivot;
                }else{
                    if(n == n->parent->r){
                        n->parent->r = pivot;
                    }else{
                        n->parent->l = pivot;
                    }
                }
                pivot->r = n;
                n->parent = pivot;
            }

            /*!
             * \brief Size of a block in the mapping. Each mapped region must be
             * at a block-granularity.
             */
            addr_t block_size_;

            /*!
             * \brief Lookup map for determining which object an address maps to
             */
            BinTreeNode* bintree_;

            /*!
             * \brief Number of mappings
             */
            uint32_t num_mappings_;

            /*!
             * \brief Vector of mappings (this is memory owned and freed by the
             * tree)
             */
            std::vector<const Mapping*> mappings_;

            /*!
             * \brief Amount to rshift an address to get a block id (for testing
             * bock spanning of accesses)
             */
            addr_t block_idx_rshift_;

            /*!
             * \brief Mask applied to an address to compute the address offset
             * from the beginning of the block containing it.
             */
            addr_t block_offset_mask_;

        }; // class SimpleMemoryMap
    }; // namespace memory
}; // namespace sparta


//! \brief SimpleMemoryMap stream operator
inline std::ostream& operator<<(std::ostream& out, const sparta::memory::SimpleMemoryMap& mi) {
    out << mi.stringize();
    return out;
}

//! \brief SimpleMemoryMap stream operator
inline std::ostream& operator<<(std::ostream& out, const sparta::memory::SimpleMemoryMap* mi) {
    if(nullptr == mi){
        out << "null";
    }else{
        out << mi->stringize();
    }
    return out;
}

#endif // __SIMPLE_MEMORY_MAP_H__
