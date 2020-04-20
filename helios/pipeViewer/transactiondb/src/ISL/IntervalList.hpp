/**
 * \file IntervalList.h
 * \author David Macke david.macke@samsung.com
 *
 * \copyright
 */

#ifndef __INTERVALLIST_H__
#define __INTERVALLIST_H__

#include "Interval.hpp"
#include <cassert>

namespace ISL {
const float P = 0.5;
extern "C" long random();
extern "C" void exit(int);

template<class IntervalT>
class IntervalList
{
public:
    /*!
     * \brief This struct is the base unit of the Interval List. At the most
     * basic point each element is just a node in a linked list.
     */
    struct IntervalListElt {
    private:
        IntervalT* loc_Int_; /*! Local Interval*/
    public:
        IntervalListElt* next;
        IntervalListElt( IntervalT* anInterval) :loc_Int_(anInterval), next(0) {
            assert( anInterval != 0 );
        }
        void set_next(IntervalListElt* nextElt) { next = nextElt; }
        IntervalListElt* get_next() const { return next; }
        IntervalT* getInterval() const { return( loc_Int_ ); }
    };

    /*! Constructor */
    IntervalList() { head_ = nullptr; };

    /*! Destructor */
    ~IntervalList() { empty(); };

    /*! Insert an interval into the interval list */
    void insert( IntervalT* I) {
        IntervalListElt* temp = new IntervalListElt( I );
        temp->next = head_;
        head_ = temp;
    }

    void remove( IntervalT* I ) {
        IntervalListElt *x, *last;
        x = head_; last = nullptr;
        while( x != nullptr && (( x->getInterval()) != I)) {
            last = x;
            x = x->next;
        }
        if( x == nullptr ) {
            return;
        } else if (last == nullptr ) {
            head_ = x->next;
            delete x;
        } else {
            last->next = x->next;
            delete x;
        }
    }

    void removeAll() {
        IntervalListElt *x;
        for( x = this->get_first(); x != nullptr; x = this->get_next(x) ) {
            remove(x->getInterval());
        }
    }

    void copy(IntervalList* from) { // add contents of "from" to self
        IntervalListElt* e = from->head_;
        while( e != nullptr ) {
            insert( e->getInterval() ) ; //reversed order?
            e = e->next;
        }
    }

    /*!
     * \brief Copies the content of this list excluding intervals which end on
     * the exclusion endpoint end_ex
     * \param from Source list to copy from
     * \param right_ex Right endpoint value which will cause any intervals
     * with a matching right endpoint not to be copied.
     */
    void copyIncExc(IntervalList* from,
                    typename IntervalT::IntervalDataT right_ex) {
        IntervalListElt* e = from->head_;
        while( e != nullptr ) {
            IntervalT* i = e->getInterval();
            if(i->getRight() != right_ex){
                insert( i ) ; //reversed order?
            }
            e = e->next;
        }
    }

    void insertUnique( IntervalT * I) { if( !contains(I)) { insert(I); } }

    bool contains( const IntervalT * I) const {
        IntervalListElt *x = head_;
        while( x != nullptr && I != x->I ) {
            x = x->next;
        }
        if( x == nullptr ) { return(false); }
        else { return(true); }
    }

    bool isEqual( IntervalList* l ) const {
        bool equal = 1;
        for( IntervalListElt* x = get_first(); x != nullptr; x = get_next(x)) {
            if( !l->constins( x->getinterval()  )) { equal = 0; }
        }
        equal = equal && ( length() == l->length() );
        return( equal );
    }

    int length() const {
        int i=0;
        for(auto x=get_first(); x!=nullptr; x=get_next(x)) { i++;}
        return(i);
    }

    void empty() { //! delete elements of self to make self an empty list.
        IntervalListElt* x = head_;
        IntervalListElt* y;
        while( x!= nullptr ) {
            y = x;
            x = x->next;
            delete y;
        };
        head_ = nullptr;
    }

    bool isEmpty() const { return( head_ == 0 ); } //! return true if list is empty

    /*! Method: return the next element in the list*/
    IntervalListElt* get_next( const IntervalListElt* element) const { return(element->next); }

    /*! Method: return the header Interval Elt*/
    IntervalListElt* get_first() const noexcept{ return head_; }

private:
    IntervalListElt* head_;
};
}
// __INTERVALLIST_H__
#endif
