// <Port> -*- C++ -*-


/**
 * \file   DataContainer.hpp
 *
 * \brief  File that defines the DataContainer class
 */

#ifndef __DATA_CONTAINER_H__
#define __DATA_CONTAINER_H__

#include "sparta/utils/ValidValue.hpp"
#include "sparta/simulation/Clock.hpp"

namespace sparta
{
    /**
     * \class DataContainer
     *
     * Used by DataInPort and SyncInPort, this class holds received
     * data from these ports and remembers the time in which the data
     * was set.  It also maintains the validity of the data.
     *
     */
    template<class DataT>
    class DataContainer
    {
    public:
        /**
         * \brief Construct the DataContainer with the clock used for
         *        timestamping.
         *
         * \param clk Clock used to get a timestamp.  Must not be nullptr
         */
        DataContainer(const sparta::Clock * clk) :
            clock_(clk)
        {
            sparta_assert(clock_ != nullptr);
        }

        /**
         * \brief Has this port received data (not timed)
         * \return true if data is live on this port
         *
         * This function only returns true is data were \b ever received
         * on this port.  It does not indicate that data was delivered
         * on \b this cycle.  For that, use dataReceivedThisCycle function.
         */
        bool dataReceived() const {
            return data_.isValid();
        }

        /**
         * \brief Has this port received data \b this cycle
         * \return true if data is live on this port and was received \b this cycle
         *
         * This function returns true is data received were
         * on this port \b this cycle.
         */
        bool dataReceivedThisCycle() const {
            return (data_.isValid() &&
                    (data_valid_time_stamp_ == clock_->getScheduler()->getCurrentTick()));
        }

        /**
         * \brief Return the last data received by the port, then clear it
         * \return The last data the port received
         * \note This function will assert if there is \b no data received.
         *
         * Pull the data from the port.  This is a destructive
         * mechanism, meaning the port is cleared
         */
        DataT pullData() {
            sparta_assert(dataReceived());
            const DataT & dat = data_.getValue();
            data_.clearValid();
            return dat;
        }

        /**
         * \brief Peek at the data in the port, but don't invalidate it
         * \return Reference to the data in the port
         * \note This function will assert if there is \b no data received
         */
        const DataT & peekData() const {
            sparta_assert(dataReceived());
            return data_.getValue();
        }

        //! \brief Clear the validity of the data at the port
        void clearData() {
            data_.clearValid();
        }

        /**
         * \brief Returns the clock cycle data was received
         * \return The clock cycle (receiver's clock) data was received
         * \note This function will assert if the data is \b not valid
         */
        Clock::Cycle getReceivedTimeStamp() const {
            sparta_assert(dataReceived());
            return clock_->getCycle(data_valid_time_stamp_);
        }

    protected:

        //! Set the data received
        void setData_(const DataT & dat) {
            data_ = dat;
            data_valid_time_stamp_ = clock_->getScheduler()->getCurrentTick();
        }

    private:

        //! The last data delivered on this port
        sparta::utils::ValidValue<DataT> data_;

        //! Timestamp when data sent last
        Scheduler::Tick data_valid_time_stamp_ = 0;

        //! Clock to use to get a timestamp
        const sparta::Clock * clock_ = nullptr;

    };
}

#endif
