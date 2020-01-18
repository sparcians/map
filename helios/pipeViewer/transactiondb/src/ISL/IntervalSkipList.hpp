/**
 * \file IntervalSkipList.h
 * \author David Macke david.macke@samsung.com
 *
 * \copyright
 **/

#ifndef __INTERVALSKIPLIST_H__
#define __INTERVALSKIPLIST_H__

#include <iostream>
#include "IntervalList.hpp"
#include "Interval.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace ISL {

    const int MAX_FORWARD = 48; // Maximum number of forward pointers
    /**
     * \brief IntervalSkipList is the main structure of the Interval Skip List
     * The interval skip list is composed of a number of interval skip list nodes.
     * Each node has a number of levels. Each level of the skip list node has a forward
     * pointer. The pointer at the lowes level points to the next node in the list. As
     * you move up the node, the pointers spans more and more nodes. Also on each level
     * of the interval skip list node is an array of markers. Each marker is an array of
     * pointers that point to the intervals that are on that level of the node.
     **/

    template<class IntervalT>
    class IntervalSkipList {
    public:
        /*! \brief IntervalSkipList is composed of IntervalSLnodes
         * The IntervalSkipList has N number of IntervalSLnodes. The first IntervalSLnode
         * is a generic IntervalSLnode that has the maximum level and all of the forward
         * pointers are set to null.
         */
        struct IntervalSLnode {
            typename IntervalT::IntervalDataT key;
            IntervalSLnode** forward;  // array of forward pointers
            IntervalList< IntervalT>**   markers;  // array of interval markers, one per pointer
            IntervalList< IntervalT>* eqMarkers;  // markers for node itself
            uint32_t ownerCount;  // number of interval end points with value equal to key
            uint32_t const topLevel;  // index of top level of forward pointers in this node.

            IntervalSLnode(typename IntervalT::IntervalDataT searchKey, uint32_t levels)
                : key(searchKey), topLevel(levels) {  //constructor
                forward = new IntervalSLnode*[levels+1];
                markers = new IntervalList< IntervalT>*[levels+1];
                eqMarkers = new IntervalList< IntervalT>();
                ownerCount = 0;
                for(uint32_t i = 0; i <= levels; i++) {
                    forward[i] = 0;
                    markers[i] = new IntervalList< IntervalT>(); // initialize empty interval list
                }
            }

            /*! Destructor*/
            ~IntervalSLnode() {
                for( uint32_t i = 0; i<=topLevel; i++ ) { delete markers[i]; }
                delete[] forward;
                delete[] markers;
                delete eqMarkers;
            }

            IntervalSLnode* get_next() const { return( forward[0] ); }

            typename IntervalT::IntervalDataT getValue() const { return( key ); }

            int level() const { return( topLevel+1 ); } // number of levels of this node

            bool isHeader() const { return( key == 0 ); } // header has NULL key value
        };

    private:
        uint32_t maxLevel_;
        IntervalSLnode* header_;

        uint32_t randomLevel_() const {
            // create a new node of random level probabistically
            uint32_t levels = 0;
            while( P < (((float) random())/ 2147483647.0)) { levels++; }
            if ( levels <= maxLevel_) { return( levels ); }
            else { return( maxLevel_+1 ); }
        }

        void placeMarkers_(IntervalSLnode* left, IntervalSLnode* right, IntervalT* Interval) {
            //! Place markers for the interval I.  left is the left endpoint of I and right is the right
            //! endpoint of I, so it isn't necessary to search to find the endpoints.
            sparta_assert(left != nullptr);
            sparta_assert(right != nullptr);

            IntervalSLnode* x = left;
            if ( Interval->contains(x->key) ) { x->eqMarkers->insert( Interval ); }
            int i = 0;  // start at level 0 and go up
            // place markers on the ascending path
            while(x->forward[i]!=0 && Interval->containsInterval(x->key,x->forward[i]->key)) {
                while(i!=x->level()-1 && x->forward[i+1] != 0
                      && Interval->containsInterval(x->key,x->forward[i+1]->key)) { i++; }
                if (x->forward[i] != 0) {
                    x->markers[i]->insert( Interval );
                    x = x->forward[i];
                    if (Interval->contains(x->key)) { x->eqMarkers->insert( Interval ); }
                }
            }
            // place markers on descending path
            while(x->key != (right->key)) {
                while(i!=0 && (x->forward[i] == 0
                               || !Interval->containsInterval(x->key,x->forward[i]->key))) { i--; }
                /**
                 * At this point, we can assert that i=0 or x->forward[i]!=0 and I contains
                 * (x->key,x->forward[i]->key).  In addition, x is between left and
                 * right so i=0 implies I contains (x->key,x->forward[i]->key).
                 * Hence, the interval must be marked.  Note that it is impossible
                 * for us to be at the end of the list because x->key is not equal
                 * to right->key.
                 **/
                x->markers[i]->insert(Interval);
                x = x->forward[i];
                if (Interval->contains(x->key)) { x->eqMarkers->insert(Interval); }
            }
        }  // end placeMarkers

        void removeMarkers_( IntervalT* Interval) {  // remove markers for Interval I
            IntervalSLnode* x = Interval->left;
            if (Interval->contains(x->key)) { x->eqMarkers->remove(Interval); }
            int i = 0;  // start at level 0 and go up
            while(x->forward[i]!=0 && Interval->containsInterval(x->key,x->forward[i]->key)) {
                // find level to take mark from
                while(i!=x->level()-1 && x->forward[i+1] != 0
                      && Interval->containsInterval(x->key,x->forward[i+1]->key)) { i++; }
                //! Remove mark from current level i edge since it is the highest edge out
                //! of x that contains I, except in the case where current level i edge
                //! is null, in which case there are no markers on it.
                if (x->forward[i] != 0) {
                    x->markers[i]->remove(Interval);
                    x = x->forward[i];
                    if (Interval->contains(x->key)) { x->eqMarkers->remove(Interval); }
                }
            }
            // remove marks from non-ascending path
            while(x->key->neq(Interval->getRight())) {
                // find level to remove mark from
                while(i!=0 && (x->forward[i] == 0
                               || !Interval->containsInterval(x->key,x->forward[i]->key))) { i--; }
                //! At this point, we can assert that i=0 or x->forward[i]!=0 and I contains
                //! (x->key,x->forward[i]->key).  In addition, x is between left and
                //! right so i=0 implies I contains (x->key,x->forward[i]->key).
                //! Hence, the interval is marked and the mark must be removed.
                //! Note that it is impossible for us to be at the end of the list
                //! because x->key is not equal to right->key.
                x->markers[i]->remove(Interval);
                x = x->forward[i];
                if (Interval->contains(x->key)) { x->eqMarkers->remove(Interval); }
            }
        }  // end removeMarkers_

        void adjustMarkersOnInsert_(IntervalSLnode* x, IntervalSLnode** update) {
            //! Phase 1:  place markers on edges leading out of x as needed.
            //! Starting at bottom level, place markers on outgoing level i edge of x. If a marker has to
            //! be promoted from level i to i+1 of higher, place it in the promoted set at each step.
            IntervalList< IntervalT> promoted;  // list of intervals that identify markers being promoted
            IntervalList< IntervalT> newPromoted; // temporary set to hold newly promoted markers
            IntervalList< IntervalT> removePromoted;  // holding place for elements to be removed
            IntervalList< IntervalT> tempMarkList;  // temporary mark list
            typename IntervalList< IntervalT>::IntervalListElt* m;
            int i;

            for( i=0; (i<= x->level() - 2) && x->forward[i+1]!=0; i++) {
                IntervalList< IntervalT>* markList = update[i]->markers[i];
                for( m = markList->get_first(); m != NULL; m = markList->get_next(m)) {
                    if(m->getInterval()->containsInterval(x->key,x->forward[i+1]->key)) {
                        // remove m from level i path from x->forward[i] to x->forward[i+1]
                        removeMarkFromLevel_(m->getInterval(),i,x->forward[i],x->forward[i+1]);
                        // add m to newPromoted
                        newPromoted.insert(m->getInterval());
                    } else {
                        // place m on the level i edge out of x
                        x->markers[i]->insert(m->getInterval());
                        // do *not* place m on x->forward[i] because it must already be there.
                    }
                }
                for( m = promoted.get_first(); m != NULL; m = promoted.get_next(m)) {
                    if(!m->getInterval()->containsInterval(x->key, x->forward[i+1]->key)) {
                        // Then m does not need to be promoted higher.
                        // Place m on the level i edge out of x and remove m from promoted.
                        x->markers[i]->insert(m->getInterval());
                        // mark x->forward[i] if needed
                        if(m->getInterval()->contains(x->forward[i]->key)) {
                            x->forward[i]->eqMarkers->insert(m->getInterval());
                        }
                        removePromoted.insert(m->getInterval());
                    } else { // continue to promote m up levels
                        removeMarkFromLevel_(m->getInterval(),i,x->forward[i],x->forward[i+1]);
                    }
                }
                promoted.removeAll();
                removePromoted.empty();
                promoted.copy(&newPromoted);
                newPromoted.empty();
            }
            //! Combine the promoted set and updated[i]->markers[i] and install them as the set of markers on the top edge out of x that is non-null.
            x->markers[i]->copy(&promoted);
            x->markers[i]->copy(update[i]->markers[i]);
            for( m=promoted.get_first(); m!=0; m=promoted.get_next(m)) {
                if(m->getInterval()->contains(x->forward[i]->key)) {
                    x->forward[i]->eqMarkers->insert(m->getInterval());
                }
            }
            //! Phase 2:  place markers on edges leading into x as needed.
            //! Markers on edges leading into x may need to be promoted as high as
            //! the top edge coming into x, but never higher.
            promoted.empty();
            for ( i=0; (i <= x->level() - 2) && !update[i+1]->isHeader(); i++) {
                tempMarkList.copy(update[i]->markers[i]);
                for( m = tempMarkList.get_first(); m != NULL; m = tempMarkList.get_next(m)) {
                    //If m needs to be promoted add m to newPtomoted
                    if(m->getInterval()->containsInterval(update[i+1]->key,x->key)) {
                        newPromoted.insert(m->getInterval());
                        // Remove m from the path of level i edges between updated[i+1]
                        // and x (it will be on all those edges or else the invariant
                        // would have previously been violated.
                        removeMarkFromLevel_(m->getInterval(),i,update[i+1],x);
                    }
                }
                tempMarkList.empty();  // reclaim storage
                for( m = promoted.get_first(); m != NULL; m = promoted.get_next(m)) {
                    if (!update[i]->isHeader() &&
                        m->getInterval()->containsInterval(update[i]->key,x->key) &&
                        !update[i+1]->isHeader() &&
                        ! m->getInterval()->containsInterval(update[i+1]->key,x->key) ) {
                        // Place m on the level i edge between update[i] and x, and remove m from promoted.
                        update[i]->markers[i]->insert(m->getInterval());
                        // mark update[i] if needed
                        if(m->getInterval()->contains(update[i]->key)) {
                            update[i]->eqMarkers->insert(m->getInterval());
                        }
                        removePromoted.insert(m->getInterval());
                    } else {
                        // Strip m from the level i path from update[i+1] to x.
                        removeMarkFromLevel_(m->getInterval(),i,update[i+1],x);
                    }
                }
                // remove non-promoted marks from promoted
                promoted.removeAll();
                removePromoted.empty();  // reclaim storage
                // add newPromoted to promoted and make newPromoted empty
                promoted.copy(&newPromoted);
                newPromoted.empty();
            };
            /*! If i=x->level()-1 then either x has only one level, or the
              top-level pointer into x must not be from the header, since
              otherwise we would have stopped on the previous iteration.
              If x has 1 level, then promoted is empty.  If x has 2 or
              more levels, and i!=x->level()-1, then the edge on the next
              level up (level i+1) is from the header.  In any of these
              cases, all markers in the promoted set should be deposited
              on the current level i edge into x.  An edge out of the
              header should never be marked.  Note that in the case where
              x has only 1 level, we try to copy the contents of the
              promoted set onto the marker set of the edge out of the
              header into x at level i=0, but of course, the promoted set
              will be empty in this case, so no markers will be placed on
              the edge.  */
            update[i]->markers[i]->copy(&promoted);
            for( m=promoted.get_first(); m!=0; m=promoted.get_next(m) ) {
                if( m->getInterval()->contains(update[i]->key) ) {
                    update[i]->eqMarkers->insert(m->getInterval());
                }
            }
            // Place markers on x for all intervals the cross x.
            for( i=0; i<x->level(); i++ ) {
                x->eqMarkers->copy(x->markers[i]);
            }
            promoted.empty(); // reclaim storage
        } // end adjustMarkersOnInsert


        /*! \brief */
        void adjustMarkersOnDelete_(IntervalSLnode* x, IntervalSLnode** update) {
            // adjust markers to prepare for deletion of x, which has vector "update"
            IntervalList< IntervalT> demoted;
            IntervalList< IntervalT> newDemoted;
            IntervalList< IntervalT> tempRemoved;
            typename IntervalList< IntervalT>::IntervalListElt* m;
            int i;
            IntervalSLnode *y;
            //! Phase 1:  lower markers on edges to the left of x as needed.
            for(i=x->level()-1; i>=0; i--) {
                // find marks on edge into x at level i to be demoted
                for(m=update[i]->markers[i]->get_first(); m!=0; m=update[i]->markers[i]->get_next(m)){
                    if(x->forward[i]==0
                       || !m->getInterval()->containsInterval(update[i]->key,x->forward[i]->key)){
                        newDemoted.insert(m->getInterval());
                    }
                }
                // Remove newly demoted marks from edge.
                update[i]->markers[i]->removeAll();
                // Place previously demoted marks on this level as needed.
                for(m=demoted.get_first(); m!=0; m=demoted.get_next(m)) {
                    //! Place mark on level i from update[i+1] to update[i], not including update[i+1] itself, since it already has a mark if it needs one.
                    for(y=update[i+1]; y!=0 && y!=update[i]; y=y->forward[i]) {
                        if (y!=update[i+1] && m->getInterval()->contains(y->key)) {
                            y->eqMarkers->insert(m->getInterval());
                        }
                        y->markers[i]->insert(m->getInterval());
                    }
                    if(y!=0 && y!=update[i+1] && m->getInterval()->contains(y->key)) {
                        y->eqMarkers->insert(m->getInterval());
                    }
                    // if lowest level m needs to be placed on, place m on the level i edge out of update[i] and remove m from the demoted set.
                    if(x->forward[i]!=0
                       && m->getInterval()->containsInterval(update[i]->key,x->forward[i]->key)) {
                        update[i]->markers[i]->insert(m->getInterval());
                        tempRemoved.insert(m->getInterval());
                    }
                }
                demoted.removeAll();
                tempRemoved.empty();
                demoted.copy(&newDemoted);
                newDemoted.empty();
            }
            //! Phase 2:  lower markers on edges to the right of D as needed
            demoted.empty();

            for( i=x->level()-1; i>=0; i--) {
                for( m=x->markers[i]->get_first(); m!=0; m=x->markers[i]->get_next(m)) {
                    if(x->forward[i]!=0 && ( update[i]->isHeader()
                                             || !m->getInterval()->containsInterval(update[i]->key,x->forward[i]->key))) {
                        newDemoted.insert(m->getInterval());
                    }
                }
                for( m=demoted.get_first(); m!=0; m=demoted.get_next(m)) {
                    // Place mark on level i from x->forward[i] to x->forward[i+1].
                    // Don't place a mark directly on x->forward[i+1] since it is already
                    // marked.
                    for( y=x->forward[i] ;y!=x->forward[i+1]; y=y->forward[i]) {
                        y->eqMarkers->insert(m->getInterval());
                        y->markers[i]->insert(m->getInterval());
                    }
                    if(x->forward[i]!=0 && !update[i]->isHeader()
                       && m->getInterval()->containsInterval(update[i]->key,x->forward[i]->key))    {
                        tempRemoved.insert(m->getInterval());
                    }
                }
                demoted.removeAll();
                demoted.copy(&newDemoted);
                newDemoted.empty();
            }
        }  // end adjustMarkersOnDelete

        void remove_(IntervalSLnode* x, IntervalSLnode** update) {
            //! remove node x, which has updated vector update then splice out x
            adjustMarkersOnDelete_(x,update);
            for(int i=0; i<=x->level()-1; i++) {
                update[i]->forward[i] = x->forward[i];
            }
            delete x;
        }

        void removeMarkers_(IntervalSLnode* left, IntervalT* Interval) {
            //! remove markers for Interval I starting at left end at right

            //! remove marks from ascending path
            IntervalSLnode* x = left;
            if( Interval->contains( x->key)) x->eqMarkers->remove( Interval );
            int i = 0;  // start at level 0 and go up
            while( x->forward[i]!=0 && Interval->containsInterval( x->key, x->forward[i]->key)) {
                // find level to take mark from
                while( i != x->level()-1
                       && x->forward[i+1] != 0
                       && Interval->containsInterval( x->key, x->forward[i+1]->key)) { i++; }
                // Remove mark from current level i edge since it is the highest edge out
                // of x that contains I, except in the case where current level i edge
                // is null, in which case there are no markers on it.
                if( x->forward[i] != 0) {
                    x->markers[i]->remove(Interval);
                    x = x->forward[i];
                    // remove I from eqMarkers set on node unless currently at right
                    // endpoint of I and I doesn't contain right endpoint.
                    if( Interval->contains( x->key) ) x->eqMarkers->remove(Interval);
                };
            };

            //! remove marks from descending path
            while( (x->key) != ( Interval->getRight())) {
                // find level to remove mark from
                while( i!=0 && ( x->forward[i] == 0 ||
                                 !Interval->containsInterval(x->key,x->forward[i]->key))) { i--; }
                // At this point, we can assert that i=0 or x->forward[i]!=0 and I contains
                // (x->key,x->forward[i]->key).  In addition, x is between left and
                // right so i=0 implies I contains (x->key,x->forward[i]->key).
                // Hence, the interval is marked and the mark must be removed.
                // Note that it is impossible for us to be at the end of the list
                // because x->key is not equal to right->key.
                x->markers[i]->remove(Interval);
                x = x->forward[i];
                if( Interval->contains(x->key)) { x->eqMarkers->remove(Interval); }
            };
        }

        void removeMarkFromLevel_(IntervalT* m, int i, IntervalSLnode* l, IntervalSLnode* r) {
            //! Remove markers for interval m from the edges and nodes on the
            //! level i path from l to r.
            IntervalSLnode *x;
            for(x=l; x!=0 && x!=r; x=x->forward[i]) {
                x->markers[i]->remove(m);
                x->eqMarkers->remove(m);
            }
            if(x!=0) { x->eqMarkers->remove(m); }
        }

        IntervalSLnode* search_( const typename IntervalT::IntervalDataT searchKey, IntervalSLnode** update) const {
            //! Search for search key, and return a pointer to the
            //! intervalSLnode x found, as well as setting the update vector
            //! showing pointers into x.
            IntervalSLnode* x = header_;
            // Find location of searchKey, build update vector indicating pointers to change on insertion
            for(int i=maxLevel_; i >= 0; i--) {
                while (x->forward[i] != 0 && (x->forward[i]->key < searchKey)) {
                    x = x->forward[i];
                }
                update[i] = x;
            }
            x = x->forward[0];
            return(x);
        }

    public:

        IntervalSkipList< IntervalT >() : maxLevel_(0) {
            header_ = new IntervalSLnode((typename IntervalT::IntervalDataT) 0, MAX_FORWARD);
            for (int i = 0; i< MAX_FORWARD; i++) { header_->forward[i] = 0; }
        }

        ~IntervalSkipList() {
            IntervalSLnode *a,*b;
            a = header_;
            while(a!=nullptr) {
                b = a->forward[0];
                delete a;
                a = b;
            }
        }

        IntervalSLnode* search( const typename IntervalT::IntervalDataT searchKey) const {
            //! return node containing Value or null
            IntervalSLnode* x = header_;
            for(int i=maxLevel_; i >= 0; i--) {
                while (x->forward[i] != 0 && x->forward[i]->key->lt(searchKey)) { x = x->forward[i]; }
            }
            x = x->forward[0];
            if(x != NULL && x->key->eq(searchKey)) { return(x); }
            else { return( NULL ); }
        }

        /**
         * \brief findIntervals expects an empty list to be passed to it. The method
         * then appends the results of the search to the List and returns
         **/
        void findIntervals( const typename IntervalT::IntervalDataT searchKey,
                            IntervalList< IntervalT> & L ) {
            //! Stabbing Query Function
            sparta_assert(L.isEmpty());
            IntervalSLnode* x = header_;
            for(int i = maxLevel_; i >= 0 && (x->isHeader() || (x->key != searchKey)); i--) {
                while (x->forward[i] != 0 && (searchKey >= (x->forward[i]->key))) {
                    x = x->forward[i];
                }
                if(!x->isHeader() && !(x->key == searchKey)) { L.copy(x->markers[i]); } //copyIncExc
                else if (!x->isHeader()) { L.copy(x->eqMarkers); } //copyIncExc
                else {}
            }
        }

        IntervalSLnode* insert(typename IntervalT::IntervalDataT searchKey) {
            //! insert a value returning a pointer
            IntervalSLnode* update[MAX_FORWARD]; // array for maintaining update pointers
            IntervalSLnode* x;
            uint32_t i;
            // Find location of searchKey, build update vector with pointers to change on insertion.
            x = search_(searchKey,update);
            if( x==0 || !(x->key == searchKey)) {
                // put a new node in the list for this searchKey
                uint32_t newLevel = randomLevel_();
                if (newLevel > maxLevel_){
                    for(i=maxLevel_+1; i<=newLevel; i++){ update[i] = header_; }
                    maxLevel_ = newLevel;
                }
                x = new IntervalSLnode(searchKey, newLevel);
                // add x to the list
                for(i=0; i<=newLevel; i++) {
                    x->forward[i] = update[i]->forward[i];
                    update[i]->forward[i] = x;
                }
                // adjust markers to maintain marker invariant
                adjustMarkersOnInsert_(x,update);
            }
            // else, the searchKey is in the list already, and x points to it.
            return(x);
        }

        void insert( IntervalT* Interval) {  //! insert an interval into list, 2 nodes per interval
            IntervalSLnode* left = insert(Interval->getLeft());
            IntervalSLnode* right = insert(Interval->getRight());
            left->ownerCount++;
            right->ownerCount++;
            placeMarkers_(left,right,Interval);
        }

        void remove( IntervalT* Interval) {  //! delete an interval from list
            IntervalSLnode* update[MAX_FORWARD]; // array for update pointers
            IntervalSLnode* left = search_(Interval->getLeft(),update);
            if( left == 0 || left->ownerCount <= 0) {
                std::cout << "ERROR: Attempt to delete an interval not in the index.(LEFT)\n";
                sparta_assert(false);
            };
            removeMarkers_( left, Interval);
            left->ownerCount--;
            if(left->ownerCount == 0) { remove_(left,update); }
            IntervalSLnode* right = search_(Interval->getRight(),update);
            if( right == 0 || right->ownerCount <= 0) {
                std::cout << "ERROR: Attempt to delete an interval not in the index.(RIGHT)\n";
                sparta_assert(false);
            };
            right->ownerCount--;
            if(right->ownerCount == 0) remove_(right,update);
        }

    };
}// NAMESPACE
// __INTERVALSKIPLIST_H__
#endif
