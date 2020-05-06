/**
 * \file IntervalWindow.h
 *
 * \brief IntervalWindow provides the data storage and search aparatus for
 *        the Helois tools.
 **/

#pragma once

#include <inttypes.h>
#include <list>
#include <string>
#include "TransactionInterval.hpp"
#include "ISL/IntervalList.hpp"
#include "ISL/IntervalSkipList.hpp"
#include "PipelineDataCallback.hpp"
#include "Reader.hpp"
#include <pthread.h>

namespace sparta {
namespace pipeViewer {

    class IntervalWindow : public PipelineDataCallback{
    private:
        /** Second thread used to maintain the active window and looading/unloading data */
        pthread_t maint_Thread_;

        /**
         *  window_L_                   active_Cycle_                 window_R_
         *      |                 |           |            |                 |
         *      |  PRE_LOAD DOWN  |           |            |   PRE_LOAD UP   |
         *      |_________________|___________|____________|_________________|
         *                        |                        |
         *            ( window_L_ + load_L_ )   ( window_R_ - load_R_ )
        **/

        uint64_t active_Cycle_;                     //! active cycle of simulation
        uint64_t loading_Range_L_,loading_Range_R_; //! Delta between previous and next window.
        uint64_t offset_L_, offset_R_;              //! offset from the active index cycle
        uint64_t load_L_, load_R_;                  //! range from window(L/R) that the window will load on the fly
        uint64_t LEC;                               //! Long Event Check, when loading intervals go LEC past window_R_
        uint64_t file_Start_, file_End_;            //! Start and End Cycle of Event file
        /*!
         * The reason for the volatile variables below is because the variables are written in
         * one thread and read in another. The window_L/R_ variables are written only in the
         * background thread. THe loading_Hold is set in the primary thread as a block if the
         * stabbing query is outside of the loaded window. The hold is removed once the window is
         * loaded in the background thread. The maintain_Interval_Run_ variable is a bool used to
         * keep the background thread running in its while loop. When m_I_R_ is set to false the
         * background thread will exit.
         */

        uint64_t volatile window_L_, window_R_;     //! window of active file
        bool volatile loading_Hold_; //! variable used to block searches if the search is outside of the loaded window
        bool volatile maintain_Interval_Run_;       //! variable used to keep background thread running when true

        /*! Main list of intervals currently inserted into the ISL*/
        std::list< transactionInterval<uint64_t>*> iarray_;
        /*! Interval Skip List used to store the intervals in this API*/
        ISL::IntervalSkipList< transactionInterval<uint64_t>> *isList;
        /*! Reader used to get intervals from the event file*/
        Reader event_Reader_;

        /*! Set the Left and Right window bounds, depending on the use within the maintainInterval() method,
         *  the setWindows() method might need to be called before or after setting the load_window variables*/
        void setWindows_() {
            if (active_Cycle_ - offset_L_ > active_Cycle_) {
                window_L_ = 0;
            }else { window_L_ = active_Cycle_ - offset_L_; }
            window_R_ = active_Cycle_ + offset_R_;
            //std::cout << "ActiveCycle "  << active_Cycle_ << " Window Now [" << window_L_ << "," << window_R_ << "] \tList Length " << iarray_.size() << "\n";
        }

        /*! Load Event data for the specified range*/
        void generateWindow_(uint64_t Left, uint64_t Right) {
            event_Reader_.getWindow(Left,Right);
        }

        /*! Method to remove all intervals from a window*/
        void clearList_() {
            std::list< transactionInterval<uint64_t>*>::iterator currentIterator;
            std::list< transactionInterval<uint64_t>*>::iterator previousIterator;
            currentIterator = iarray_.begin();
            while( currentIterator != iarray_.end()) {
                isList->remove(*currentIterator);
                previousIterator = currentIterator++;
                delete *previousIterator;
                iarray_.erase(previousIterator);
            }
        }

        /*! Method to trim the window of data down to [window_L_,window_R_]*/
        void trimList_() {
            std::list< transactionInterval<uint64_t>*>::iterator currentIterator;
            std::list< transactionInterval<uint64_t>*>::iterator previousIterator;
            currentIterator = iarray_.begin();
            /** run through all the current intervals in the list iarray_*/
            while( currentIterator != iarray_.end()) {
                /*! test to see if the current interval is outside of the current window. If the interval
                      is outside the current range, the interval is removed from the ISL and from iarray_*/
                if(( ((*currentIterator)->getLeft()) > window_R_) || ( ((*currentIterator)->getRight()) < window_L_)) {
                    isList->remove(*currentIterator);
                    previousIterator = currentIterator++;
                    delete *previousIterator;
                    iarray_.erase(previousIterator);
                } else {
                ++currentIterator;
                }
            }
        }

        /*! background method used to maintain the data window [window_L_,window_R_]*/
        void maintainInterval_() {
            /** Loop used to keep the background thread running*/
            while(maintain_Interval_Run_ == true){
                /** If to check if the window_L_ is equal to 0. This if is used to prevent rolling
                      of the uint64_t. */
                if(window_L_ == 0) {
                    if( (active_Cycle_ - offset_L_ >= active_Cycle_)      /* uint64_t roll danger */
                     && ((window_R_+ 1) < (active_Cycle_ + offset_R_)) ){ /* make sure active_Cycle_ moving */
                    // danger of rolling the window_L_
                        loading_Range_L_ = ++window_R_;
                        loading_Range_R_ = active_Cycle_ + offset_R_;
                        setWindows_();
                        generateWindow_( loading_Range_L_ , loading_Range_R_ );
                    }else if( active_Cycle_ - offset_L_ < active_Cycle_ ) {
                    // uint doesn't roll
                        loading_Range_L_ = ++window_R_;
                        loading_Range_R_ = active_Cycle_ + offset_R_;
                        setWindows_();
                        generateWindow_( loading_Range_L_ , loading_Range_R_ );
                    }else{ }
                    trimList_();
                /** The else is the case for anytime the window is beyond the beginning of
                      the event file. Once window_L_ > 0, the else case is used.*/
                }else {
                    if( active_Cycle_ <= window_L_) {
                    // New window to the left of the previous
                        setWindows_();
                        loading_Range_L_ = window_L_;
                        loading_Range_R_ = window_R_;
                        generateWindow_( loading_Range_L_ , loading_Range_R_ );
                    }else if( (active_Cycle_ <= (window_L_ + load_L_)) && (active_Cycle_ > window_L_) ) {
                    // Sliding the original window to the left
                        if( active_Cycle_ - offset_L_ > active_Cycle_)
                        { loading_Range_L_ = 0;
                        }else{ loading_Range_L_ = active_Cycle_ - offset_L_;}
                        loading_Range_R_ = --window_L_;
                        setWindows_();
                        generateWindow_( loading_Range_L_ , loading_Range_R_ );
                    }else if( (active_Cycle_ > (window_L_ + load_L_)) && (active_Cycle_ < (window_R_ - load_R_)) ) {
                    // Area of no action within the center of the window
                    }else if( (active_Cycle_ >= (window_R_ - load_R_)) && (active_Cycle_ < window_R_) ) {
                    // Sliding the original window to the right
                        loading_Range_L_ = ++window_R_;
                        loading_Range_R_ = active_Cycle_ + offset_R_;
                        setWindows_();
                        generateWindow_( loading_Range_L_ , loading_Range_R_ );
                    }else if( active_Cycle_ >= window_R_ ) {
                    // New window to the right of the previous
                        setWindows_();
                        loading_Range_L_ = window_L_;
                        loading_Range_R_ = window_R_;
                        generateWindow_( loading_Range_L_ , loading_Range_R_ );
                    }else {}
                    trimList_();
                }
                loading_Hold_ = false;
                usleep(10);
            } // while(maintain_Interval_Run_ == true)
            clearList_();
        }

        /*! Helper function to start the background thread running.*/
        static void* threadProc_(void* argc) {
            static_cast<IntervalWindow*>(argc)->maintainInterval_();
            return(NULL);
        }

        void foundTransactionRecord(transaction_t* loc) {
            if((loc->time_End <= loading_Range_R_) && (loc->time_End > loading_Range_L_)){
                transactionInterval<uint64_t> *temp;
                temp = new transactionInterval<uint64_t>(loc->time_Start, loc->time_End, loc->control_Process_ID,
                                                         loc->transaction_ID, loc->display_ID, loc->location_ID,
                                                         loc->flags);
                iarray_.push_back(temp);
                isList->insert(temp);
            }
        }
        void foundInstRecord(instruction_t* loc) {
            if((loc->time_End <= loading_Range_R_) && (loc->time_End > loading_Range_L_)){
                transactionInterval<uint64_t> *temp;
                temp = new transactionInterval<uint64_t>(loc->time_Start, loc->time_End, loc->control_Process_ID,
                                                         loc->transaction_ID, loc->display_ID, loc->location_ID,
                                                         loc->flags,
                                                         loc->parent_ID, loc->operation_Code, loc->virtual_ADR,
                                                         loc->real_ADR);
                iarray_.push_back(temp);
                isList->insert(temp);
            }
        }
        void foundMemRecord(memoryoperation_t* loc) {
            if((loc->time_End <= loading_Range_R_) && (loc->time_End > loading_Range_L_)){
                transactionInterval<uint64_t> *temp;
                temp = new transactionInterval<uint64_t>(loc->time_Start, loc->time_End, loc->control_Process_ID,
                                                         loc->transaction_ID, loc->display_ID, loc->location_ID,
                                                         loc->flags,
                                                         loc->parent_ID, loc->virtual_ADR, loc->real_ADR);
                iarray_.push_back(temp);
                isList->insert(temp);
            }
        }

        void foundPairRecord(pair_t* loc) {
            if((loc->time_End <= loading_Range_R_ ) && (loc->time_End > loading_Range_L_)) {
                transactionInterval<uint64_t> *temp;
                temp = new transactionInterval<uint64_t>(loc->time_Start, loc->time_End, loc->control_Process_ID,
                                                         loc->transaction_ID, loc->display_ID, loc->location_ID, loc->flags,
                                                         loc->parent_ID, loc->length, loc->pairId,
                                                         loc->sizeOfVector, loc->valueVector, loc->nameVector,
                                                         loc->stringVector, loc->delimVector);
                iarray_.push_back(temp);
                isList->insert(temp);
            }
        }

        void foundAnnotationRecord(annotation_t* loc) {
            if((loc->time_End <= loading_Range_R_) && (loc->time_End > loading_Range_L_)){
                transactionInterval<uint64_t> *temp;
                temp = new transactionInterval<uint64_t>(loc->time_Start, loc->time_End, loc->control_Process_ID,
                                                         loc->transaction_ID, loc->display_ID, loc->location_ID,
                                                         loc->flags,
                                                         loc->parent_ID, loc->length, (char*)loc->annt);
                iarray_.push_back(temp);
                isList->insert(temp);
            }
        }

    public:

        /*! Constructor*/
        IntervalWindow(std::string filename) : event_Reader_( filename , this) {
            isList = new ISL::IntervalSkipList< transactionInterval<uint64_t>>;
            offset_L_ = 5000000;
            offset_R_ = 5000000;
            load_L_ = 4000000;
            load_R_ = 4000000;
            LEC = 1000;
            active_Cycle_ = 0;
            maintain_Interval_Run_ = true;
            file_Start_ = event_Reader_.getCycleFirst();
            file_End_ = event_Reader_.getCycleLast();
            setWindows_();
            loading_Range_L_ = window_L_;
            loading_Range_R_ = window_R_ + LEC;
            generateWindow_(window_L_,window_R_);
            pthread_create(&maint_Thread_, NULL, threadProc_ , this);
        }

        /*! Destructor*/
        virtual ~IntervalWindow() {
            maintain_Interval_Run_ = false;
            pthread_join(maint_Thread_, NULL);
            //std::cout << "IntervalWindow background Thread Closed\n";
            delete isList;
        }

        /*! Set the number of cycles to buffer to the left of the currently active cycle*/
        void setOffsetLeft( uint64_t sOL ) { offset_L_ = sOL; }
        /*! Set the number of cycles to buffer to the right of the currently active cycle*/
        void setOffsetRight( uint64_t sOR ) { offset_R_ = sOR; }
        /*! Set the left range to load section*/
        void setLoadLeft( uint64_t lwd ) {
            if( lwd > offset_L_) { load_L_ = offset_L_ - 10;
            }else{ load_L_ = lwd; }
        }
        /*! Set the right range to load section*/
        void setLoadRight( uint64_t rwd ) {
            if( rwd > offset_R_) { load_R_ = offset_R_ - 10;
            }else{ load_R_ = rwd; }
        }
        /*! Set the End Of List value*/
        void setLEC( uint64_t lec ) { LEC = lec; }
        /*! Query the Lower cycle bound of the Window*/
        uint64_t getWindowL() const { return( window_L_); }
        /*! Query the Upper cycle bound of the Window*/
        uint64_t getWindowR() const { return( window_R_); }
        /*! Get the lowest start cycle in the event file*/
        uint64_t getFileStart() const { return( file_Start_ ); }
        /*! Get the highest end cycle in the event file*/
        uint64_t getFileEnd() const { return( file_End_ ); }

        /*! Query populates the list with events at interval "qClock"*/
        void stabbingQuery(uint64_t qClock, ISL::IntervalList< transactionInterval<uint64_t>> &List) {
            active_Cycle_ = qClock;
            usleep(10);
            //! If the stabbing query is outside the window blokc search until the window loads.
            if( (active_Cycle_ < window_L_) || (active_Cycle_ > window_R_) ){
                loading_Hold_ = true;
                std::cout << "Active cycle is outside window, regenerating...\n";
                while( loading_Hold_ == true ) { usleep(10); }
            }else{}
            isList->findIntervals( active_Cycle_, List);
        }
    };// CLASS:IntervalWindow
}// NAMESPACE:pipeViewer
}// NAMESPACE:Sparta
