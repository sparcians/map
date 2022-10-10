#pragma once

#include <vector>
#include <cinttypes>
#include <string>
#include <map>
#include <list>

#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/events/PayloadEvent.hpp"

namespace sparta
{
    class ScoreboardView;

    /**
     * \class Scoreboard
     *
     * The Scoreboard of the model simply keeps track of the readiness
     * of phyiscal registers in the OOO core.  There are two parts to the SB:
     *
     * # The Scoreboard or "master" for each register file type (GPU,
     *   FPR, Vector, etc).  Typically a Rename block is responsible
     *   for setting/clearing the SB readiness.
     *
     * # The ScoreboardView is created by a Scheduling/Execution block
     *   and is used to determine if an instruction is ready for
     *   execution (all operands ready)
     *
     */
    class Scoreboard : public sparta::Unit
    {
    public:
        using UnitID = uint32_t;
        using InstID = uint64_t;

        static constexpr UnitID INVALID_UNIT_ID   = std::numeric_limits<UnitID>::max();
        static constexpr uint32_t INVALID_LATENCY = static_cast<uint32_t>(-1);
        static constexpr uint32_t MAX_REGISTERS   = 512;

        using RegisterBitMask = std::bitset<MAX_REGISTERS>;

        //! \brief Name of this resource. Required by sparta::ResourceFactory
        static const char name[];

        class ScoreboardParameters : public sparta::ParameterSet
        {
        public:
            using LatencyMatrixParameterType = std::vector<std::vector<std::string>>;

            explicit ScoreboardParameters(sparta::TreeNode * n);

            //
            // Example table:
            //
            // [  # FROM
            //    #  |
            //    #  V
            //    [""     ,"ALU0", "ALU1",   "LSU",   "FPU"], # <- TO
            //    ["ALU0",    "0",    "1",     "1",     "3"],
            //    ["ALU1",    "1",    "0",     "1",     "3"],
            //    ["LSU",     "1",    "1",     "0",     "1"],
            //    ["FPU",     "3",    "3",     "1",     "0"]
            // ]
            //
            PARAMETER(LatencyMatrixParameterType, latency_matrix, {},
                      "The forwarding latency matrix.  See the Scoreboard test for format example")
        };

        /**
         * \brief Construct a Scoreboard
         *
         * \param container The TreeNode this Scoreboard belongs to
         * \param params The parameters of this Scoreboard
         *
         */
        Scoreboard(sparta::TreeNode * container, const ScoreboardParameters * params);

        /**
         * \brief Set Ready bits on the master scoreboard
         * \param bits Bits to set as ready
         *
         * A bit value of 1 means the rename is ready, 0 is not.  When
         * this function is called, it will propogate those ready bits
         * to all registered ScoreboardViews in the machine _immediately_.
         */
        void set(const RegisterBitMask & bits);

        /**
         * \brief Set Ready bits on the master scoreboard
         * \param bits Bits to set as ready
         * \param producer The unit ID of the producer
         *
         * A bit value of 1 means the rename is ready, 0 is not.  When
         * this function is called, it will propogate those ready bits
         * to all registered ScoreboardViews in the machine using the
         * registered latencies.
         */
        void set(const RegisterBitMask & bits, UnitID producer);

        /**
         * \brief Clear the given bits from the Scoreboard
         * \param bits The bits to clear
         *
         * A bit value of 1 is cleared (sets the Scoreboard internal
         * value to 0).  This is propogated to all of the Scoreboard
         * Views immediately.
         */
        void clearBits(const RegisterBitMask & bits);

        /**
         * \brief Register a view with this Scoreboard based on
         *        producer's name.  The producer name MUST be listed
         *        in column 0 of the latency_matrix parameter.
         * \param producer_name The name of the producer
         * \param view The view to register
         */
        UnitID registerView(const std::string & producer_name,
                            ScoreboardView * view);

        //! Look at the master scoreboard's view of ready.
        bool isSet(const RegisterBitMask & bits) const;

    private:

        // Allow the ScoreboardView to access the above structure
        friend class ScoreboardView;

        // 0 means the register is not ready.
        RegisterBitMask global_reg_ready_mask_{0xffffffff}; ///< actual ready

        // Setting up the forwarding matrix based on integer lookups
        using ForwardingLatency          = uint32_t;
        using ForwardingLatencyConsumers = std::vector<ForwardingLatency>;
        using ForwardingLatencyProducers = std::vector<ForwardingLatencyConsumers>;

        // A vector of producers, with each producing row having a list of consumers
        ForwardingLatencyProducers forwarding_latencies_;

        // Units found in the latency table, first column, second row onward
        using UnitToIDMap = std::map<std::string, uint32_t>;
        UnitToIDMap unit_name_to_id_;

        // UnitID to the ScoreboardViews
        // Single unit can have multiple ScorboardViews
        using UnitIDToSBVs = std::vector<std::vector<ScoreboardView *>>;
        UnitIDToSBVs unit_id_to_scoreboard_views_;

        // Producer UnitID to consumer ScoreboardViews
        using ConsumerSBV = std::tuple<ScoreboardView *, ForwardingLatency>;
        using ConsumerSBVs = std::vector<ConsumerSBV>;
        using ProducerToConsumerSBVs = std::vector<ConsumerSBVs>;
        ProducerToConsumerSBVs producer_to_consumer_scoreboard_views_;

        // Unit ID count
        uint32_t unit_id_ = 0;

        // Structure to hold a forwarded scoreboard update
        struct ScoreboardUpdate {
            RegisterBitMask bits;
            UnitID    producer;
        };

        // PayloadEvent used to deliver the Scoreboard contents to the views
        struct ScoreboardViewUpdate
        {
            ScoreboardUpdate first;
            ScoreboardView *second;
            bool is_canceled;
        };
        sparta::PayloadEvent<ScoreboardViewUpdate, sparta::SchedulingPhase::Update> scoreboard_view_updates_;
        void deliverScoreboardUpdate_(const ScoreboardViewUpdate &);
    };

    /**
     * \class ScoreboardView
     * \brief A ScoreboardView is a view into the master Scoreboard for operand readiness
     *
     * Used by the Schedulers/Execution units, the view represents
     * that Scheduler/Execution unit's view into readiness of a rename
     * in the machine.
     */
    class ScoreboardView
    {
    public:
        //! Typedef for the callbacks
        using ReadinessCallback = std::function<void(const Scoreboard::RegisterBitMask&)>;

        /**
         * \brief Create a ScoreboardView
         *
         * \param unit_name The unit name that's creating/receiving SB updates
         * \param scoreboard_type The type of master Scoreboard to connect to
         * \param The sparta::TreeNode to search for the scoreboard_type
         */
        ScoreboardView(const std::string & unit_name,
                       const std::string & scoreboard_type,
                       sparta::TreeNode * node);

        /**
         * \brief Register a ready callback to be called when the bits are ready
         *
         * \param bits RegisterBitMask of bits to masked to determine ready
         * \param callback The handler to call; boolean argument + bits being set
         *
         * Expected callback signature:
         *
         *    void func(const Scoreboard::RegisterBitMask & bits);
         *
         * After a scoreboard update and the new bits are a match for
         * the registered bits, the callback will be called and
         * cleared from the Scoreboard.
         */
        void registerReadyCallback(const Scoreboard::RegisterBitMask & bits,
                                   const Scoreboard::InstID inst_id,
                                   const ReadinessCallback & callback);

        /**
         * \brief On a flush any registered callback needs to be "forgotten"
         *
         * \param unique_id The unique ID to find and flush
         *
         * Clears ready callbacks
         */
        void clearCallbacks(const Scoreboard::InstID inst_id);

        /**
         * \brief See if the given bits are set
         * \param bits Bits to check
         * \return A boolean of result
         */
        bool isSet(const Scoreboard::RegisterBitMask & bits) const
        {
            return bits == (local_ready_mask_ & bits);
        }

        /**
         * \brief Set the given bits as ready in the Scoreboard
         * \param bits Bits to propagate
         */
        void setReady(const Scoreboard::RegisterBitMask & bits);

        /**
         * \brief Get the scoreboard type identifier
         * \return Name string for the scoreboard type
         */
        std::string getType() const {
            return scoreboard_type_;
        }

    private:

        // Make the Scoreboard a friend to clear bits in the view
        friend Scoreboard;

        /**
         * \brief Called by the Scoreboard, when the ready bits are set for
         *        an operand, this function will be called.
         * \param bits The bits to be "or"ed in
         * \param producer The producer of the scoreboard updates
         */
        void receiveScoreboardUpdate_(const Scoreboard::RegisterBitMask & bits,
                                      const Scoreboard::UnitID producer);

        // Find a Scoreboard that matches this view in the node hierarchy
        Scoreboard::UnitID findMasterScoreboard_(const std::string & unit_name,
                                                 const std::string & scoreboard_type,
                                                 sparta::TreeNode * parent);

        /**
         * \brief Called by the Scoreboard, bits are cleared.
         * \param bits The bits to clear
         */
        void clearBits_(const Scoreboard::RegisterBitMask & bits);

        Scoreboard::RegisterBitMask local_ready_mask_{0xffffffff};  ///< actual ready

        //! Pointer to the master scoreboard
        Scoreboard * master_scoreboard_ = nullptr;

        struct CallbackData
        {
            CallbackData(const Scoreboard::RegisterBitMask & bv,
                         const Scoreboard::InstID iid,
                         const ReadinessCallback & cb,
                         sparta::Clock::Cycle cyc) :
                needed_bits(bv), inst_id(iid), callback(cb), registered_time(cyc)

            {}

            CallbackData(CallbackData&&) = default;

            const Scoreboard::RegisterBitMask needed_bits;
            const Scoreboard::InstID          inst_id;
            ReadinessCallback                 callback;
            sparta::Clock::Cycle              registered_time;
        };

        using ReadinessCallbacks = std::list<CallbackData>;
        ReadinessCallbacks ready_callbacks_;

        const sparta::Clock * clock_;
        const Scoreboard::UnitID unit_id_;
        const std::string scoreboard_type_;
    };

    // This override (implementation in Scoreboard.cpp) will provide a
    // friendlier scoreboard output
    extern std::string printBitSet(const Scoreboard::RegisterBitMask & bits);
}
