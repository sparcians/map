// <SharedData.h> -*- C++ -*-


/**
 * \file SharedData.hpp
 * \brief Defines the SharedData class
 *
 */

#ifndef __SHARED_DATA_H__
#define __SHARED_DATA_H__

#include <array>
#include "sparta/utils/ValidValue.hpp"
#include "sparta/events/GlobalEvent.hpp"

namespace sparta
{
    /*!
     * \class SharedData
     *
     * \brief Class that allows the writing of data \b this cycle, but
     *        not visable until \b next cycle
     *
     * \tparam DataT The data to share
     * \tparam manual_update Is this SharedData object manually
     *                       updated? Default is false
     *
     * This class supports two views of data: current view and the
     * next cycle view.  This class represents a latch concept.
     *
     * For auto-updates, the SharedData item will propogate the next
     * state value to the present state between clock cycles.  This
     * will occur only once per write(), and the present state value
     * will be clobbered.
     */
    template<class DataT, bool manual_update = false>
    class SharedData
    {
        // Convenient typedef for internal data type
        typedef std::array<utils::ValidValue<DataT>, 2> PSNSData;

        // Next state index
        uint32_t NState_() const {
            return (current_state_ + 1) & 0x1;
        }

        // Present state index
        uint32_t PState_() const {
            return current_state_;
        }

    public:
        /**
         * \brief Construct a SharedData item
         * \param name The name of this SharedData object
         * \param clk The clock it uses to advance the internal data
         * \param init_val The initial value of SharedData item
         */
        template<typename U = DataT>
        SharedData(const std::string & name,
                   const Clock * clk,
                   U && init_val = U()) :
            ev_update_(clk, CREATE_SPARTA_HANDLER(SharedData, update_))
        {
            writePS(std::forward<U>(init_val));
        }

        /**
         * \brief Write data to the current view
         * \param dat The data to write for visibility \b this cycle
         */
        void writePS(const DataT & dat) {
            writePSImpl_(dat);
        }

        /**
         * \brief Write data to the current view
         * \param dat The data to write for visibility \b this cycle
         */
        void writePS(DataT && dat) {
            writePSImpl_(std::move(dat));
        }

        /**
         * \brief Is there data in the current view
         * \return true if data is valid \b this cycle
         */
        bool isValid() const {
            return data_[PState_()].isValid();
        }

        /**
         * \brief Get a constant reference to the data visible this cycle
         * \return Read data in the current view.  Must be valid
         * \throw SpartaException if data is not valid
         */
        const DataT & read() const {
            sparta_assert(isValid());
            return data_[PState_()];
        }

        /**
         * \brief Get a non-constant reference to the data visible this cycle
         * \return Read data in the current view.  Must be valid
         * \throw SpartaException if data is not valid
         */
        DataT & access() {
            sparta_assert(isValid());
            return data_[PState_()];
        }

        /**
         * \brief Write data for the next cycle view
         * \param dat The data to write for visibility \b next cycle
         */
        void write(const DataT & dat) {
            writeImpl_(dat);
        }

        /**
         * \brief Write data for the next cycle view
         * \param dat The data to write for visibility \b next cycle
         */
        void write(DataT && dat) {
            writeImpl_(std::move(dat));
        }

        /**
         * \brief Is there data for the \b next cycle?
         * \return true if data is coming up
         */
        bool isValidNS() const {
            return data_[NState_()].isValid();
        }

        /**
         * \brief Get a constant reference to the data that \b
         *        will \b be visible next cycle
         * \return Data coming up in the next cycle
         * \throw SpartaException if data is not valid
         */
        const DataT & readNS() const {
            sparta_assert(isValidNS());
            return data_[NState_()];
        }

        /**
         * \brief Get a non-constant reference to the data that \b
         *        will \b be visible next cycle
         * \return Data coming up in the next cycle
         * \throw SpartaException if data is not valid
         */
        DataT & accessNS() {
            sparta_assert(isValidNS());
            return data_[NState_()];
        }

        //! \brief Clear both Present State and Next State valids
        void clear() {
            data_[PState_()].clearValid();
            data_[NState_()].clearValid();
        }

        //! \brief Clear Next State valid
        void clearNS() {
            data_[NState_()].clearValid();
        }

        //! \brief Clear Present State valid
        void clearPS() {
            data_[PState_()].clearValid();
        }

        //! \brief Update the SharedData class -- move next cycle data
        //! to current view.  Can only be called on a manually updated
        //! SharedData class
        void update() {
            static_assert(manual_update == true,
                          "Cannot call update on a SharedData object if manual_update == false");
            update_();
        }

    private:
        template<typename U>
        void writePSImpl_(U && dat) {
            data_[PState_()] = std::forward<U>(dat);
        }

        template<typename U>
        void writeImpl_(U && dat) {
            data_[NState_()] = std::forward<U>(dat);
            if constexpr (!manual_update) {
                ev_update_.schedule(1);
            }
        }

        void update_()
        {
            current_state_ = NState_();
            clearNS();
        }

        // Global Event
        GlobalEvent<SchedulingPhase::Update> ev_update_;

        // Current state
        uint32_t current_state_ = 0;

        // The PS/NS data
        PSNSData data_;
    };
}

// __SHARED_DATA_H__
#endif
