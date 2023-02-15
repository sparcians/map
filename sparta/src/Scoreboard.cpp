
#include "sparta/resources/Scoreboard.hpp"

namespace sparta
{
    const char Scoreboard::name[] = "Scoreboard";

    Scoreboard::ScoreboardParameters::ScoreboardParameters(sparta::TreeNode * n) :
        sparta::ParameterSet(n)
    {
        auto matrix_validity = [](LatencyMatrixParameterType & lat_matrix,
                                  const sparta::TreeNode *n) -> bool
        {
            // The latency matrix should have this format:
            //     Producer
            //       |
            //       V
            //           "",   "unit_name0", "unit_name1", "unit_name2"  <- Consumer
            //   "unit_name0",    "1"      ,    "1"      ,    "1"
            //   "unit_name1",    "1"      ,    "1"      ,    "1"
            //   "unit_name2",    "1"      ,    "1"      ,    "1"

            // Check for empty matrix
            if(lat_matrix.size() == 0) {
                std::cerr << "ERROR: " << n->getLocation()
                          << ": Latency matrix is empty" << std::endl;
                return false;
            }

            // Check for a matrix that's a LEAST 2x2
            if(lat_matrix.size() < 2) {
                std::cerr << "ERROR: " << n->getLocation()
                          << ": Latency matrix should be at least 2x2" << std::endl;
                return false;
            }
            else {
                // Check the two rows
                if(lat_matrix[0].size() < 2 || lat_matrix[1].size() < 2) {
                    std::cerr << "ERROR: " << n->getLocation()
                              << ": Latency matrix should be at least 2x2" << std::endl;
                    return false;
                }
            }

            // Check to make sure rows and cols line up
            const size_t num_rows = lat_matrix.size();
            for(uint32_t r = 0; r < num_rows; ++r)
            {
                if(num_rows != lat_matrix[r].size()) {
                    std::cerr << "ERROR: " << n->getLocation() <<
                        ": Missing a column on row " << r + 1 <<
                        " of latency matrix" << std::endl;
                    return false;
                }
            }

            // Check for 1-1 correspondance of the unit names.
            for(uint32_t pt = 1; pt < num_rows; ++pt) {
                const std::string producer_name = lat_matrix[pt][0];
                const std::string consumer_name = lat_matrix[0][pt];
                if(producer_name != consumer_name) {
                    std::cerr << "ERROR: " << n->getLocation()
                              << ": Mismatch on producer/consumer names. producer: '"
                              << producer_name << "' consumer '" << consumer_name << std::endl;
                    return false;
                }
            }

            // Check for valid integers in the table
            const size_t num_cols = num_rows;
            for(uint32_t row = 1; row < num_rows; ++row) {
                for(uint32_t col = 1; col < num_cols; ++col)
                {
                    try {
                        (void)std::stol(lat_matrix[row][col], nullptr, 0);
                    }
                    catch(...) {
                        std::cerr << "ERROR: " << n->getLocation()
                                  << ": This is not an integer: '"
                                  << lat_matrix[row][col]
                                  << "' on row " << row
                                  << " col " << col << std::endl;
                        return false;
                    }
                }
            }

            // all good
            return true;
        };
        latency_matrix.addDependentValidationCallback(matrix_validity,
                                                      "Issues setting the latency matrix");
    }

    Scoreboard::Scoreboard(sparta::TreeNode * parent,
                           const ScoreboardParameters * params) :
        sparta::Unit(parent),
        scoreboard_view_updates_(getEventSet(), parent->getName() + "update_payload_event",
                                 CREATE_SPARTA_HANDLER_WITH_DATA(Scoreboard, deliverScoreboardUpdate_, ScoreboardViewUpdate))
    {
        // Set up the forwarding latency table rows size to be as
        // large as the number of rows in the matrix (minus 1 for the
        // header)
        const auto num_producers = params->latency_matrix.getValue().size() - 1;
        forwarding_latencies_.resize(num_producers);
        unit_id_to_scoreboard_views_.resize(num_producers);
        producer_to_consumer_scoreboard_views_.resize(num_producers);

        // Skip the first row -- it's a header
        for (uint32_t producer_row_idx = 1;
             producer_row_idx < params->latency_matrix.getValue().size();
             ++producer_row_idx)
        {
            const auto & producer_row = params->latency_matrix.getValue()[producer_row_idx];

            const auto producer_name = producer_row[0];

            // Remember the producer name and assign an ID to it
            sparta_assert(unit_name_to_id_.find(producer_name) == unit_name_to_id_.end(),
                          "Unit name already is in the latency table (column 0) twice: "
                          << producer_name);
            const uint32_t producer_id = unit_id_++;
            unit_name_to_id_[producer_name] = producer_id;

            // Set up the forwarding latency table column size to be
            // as large as the number of columns in the matrix (minus 1
            // for the producer names)
            forwarding_latencies_[producer_id].resize(producer_row.size() - 1);

            // Set up the producer -> consumer latency table
            // Skip the first column, it's the producer name
            for (uint32_t consumer_column_idx = 1; consumer_column_idx < producer_row.size(); ++consumer_column_idx)
            {
                try {
                    forwarding_latencies_[producer_id][consumer_column_idx - 1] =
                        std::stol(producer_row[consumer_column_idx]);
                }
                catch(...) {
                    sparta_assert(false, "Error while trying to convert " <<
                                  producer_row[consumer_column_idx] << " to an int. Row "
                                  << producer_row_idx
                                  << " Column "
                                  << consumer_column_idx);
                }
            }
        }

    }

    void Scoreboard::set(const Scoreboard::RegisterBitMask & bits)
    {
        // Update registered scoreboards immediately
        for(auto & sbvs : unit_id_to_scoreboard_views_)
        {
            for (auto * sbv : sbvs)
            {
                sbv->receiveScoreboardUpdate_(bits, INVALID_UNIT_ID);
            }
        }
    }

    void Scoreboard::set(const Scoreboard::RegisterBitMask & bits, Scoreboard::UnitID producer)
    {
        sparta_assert(producer < forwarding_latencies_.size(),
                      "could not find producer ID in forwarding_latencies table");

        for(auto [sbv, latency] : producer_to_consumer_scoreboard_views_[producer])
        {
            if(latency != 0) {
                scoreboard_view_updates_.
                    preparePayload(ScoreboardViewUpdate{{bits, producer}, sbv, false})->schedule(latency);
            } else {
                sbv->receiveScoreboardUpdate_(bits, producer);
            }
        }
    }

    void Scoreboard::clearBits(const Scoreboard::RegisterBitMask & bits)
    {
        const auto inv_bits     = ~bits;
        global_reg_ready_mask_ &= inv_bits;

        // Update registered scoreboards
        for(auto & sbvs : unit_id_to_scoreboard_views_) {
            for (auto * sbv : sbvs) {
                sbv->clearBits_(bits);
            }
        }
    }

    bool Scoreboard::isSet(const Scoreboard::RegisterBitMask & bits) const {
        return (global_reg_ready_mask_ & bits) == bits;
    }

    Scoreboard::UnitID Scoreboard::registerView(const std::string & producer_name,
                                                ScoreboardView * view)
    {
        auto it = unit_name_to_id_.find(producer_name);
        sparta_assert(it != unit_name_to_id_.end(),
                      "Error: " << producer_name << " not found in scoreboard "
                      << getContainer()->getLocation());
        // Setup the mapping from Unit ID to scoreboard view
        const auto unit_id = it->second;
        sparta_assert(unit_id < unit_id_to_scoreboard_views_.size(),
                      "Ack!  Bug in Scoreboard registerView.  The unit_name_to_id_ map "
                      "is outta wack with with the unit_id_to_scoreboard_views_ vector");
        unit_id_to_scoreboard_views_[unit_id].emplace_back(view);
        view->receiveScoreboardUpdate_(Scoreboard::RegisterBitMask({global_reg_ready_mask_}), INVALID_UNIT_ID);
        // Setup the mapping from other producers to this scoreboard view as consumer
        for(uint32_t producer = 0; producer < producer_to_consumer_scoreboard_views_.size(); ++producer)
        {
            if(const auto latency = forwarding_latencies_[producer][unit_id];
               latency != INVALID_LATENCY) {
               producer_to_consumer_scoreboard_views_[producer].emplace_back(std::make_tuple(view, latency));
            }
        }
        return unit_id;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Payload receiving methods
    void Scoreboard::deliverScoreboardUpdate_(const ScoreboardViewUpdate & update)
    {
        update.second->receiveScoreboardUpdate_(update.first.bits, update.first.producer);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // ScoreboardView implementation
    ////////////////////////////////////////////////////////////////////////////////

    ScoreboardView::ScoreboardView(const std::string & unit_name,
                                   const std::string & scoreboard_type,
                                   sparta::TreeNode * parent) :
        clock_(parent->getClock()),
        unit_id_(findMasterScoreboard_(unit_name, scoreboard_type, parent)),
        scoreboard_type_(scoreboard_type)
    {
    }

    // Tell the master scoreboard that the operands are ready
    void ScoreboardView::setReady(const Scoreboard::RegisterBitMask & bits)
    {
        if (bits.none()) {
            return; // setting zero bits ready (can happen with stores and other no dest ops)
        }

        master_scoreboard_->set(bits, unit_id_);
    }

    void ScoreboardView::receiveScoreboardUpdate_(const Scoreboard::RegisterBitMask & bits,
                                                  const Scoreboard::UnitID producer)
    {
        sparta_assert(bits.any(),
                      "Update should only be generated for non-empty vector");

        // Setting local ready bits
        local_ready_mask_ |= bits;

        auto cbit = ready_callbacks_.begin();
        const auto eit = ready_callbacks_.end();
        while(cbit != eit)
        {
            const auto & cb_data = *cbit;
            if (cb_data.needed_bits == (local_ready_mask_ & cb_data.needed_bits))
            {
                cb_data.callback(bits);
                cbit = ready_callbacks_.erase(cbit);
                continue;
            }
            ++cbit;
        }
    }

    void ScoreboardView::registerReadyCallback(const Scoreboard::RegisterBitMask & bits,
                                               const Scoreboard::InstID inst_id,
                                               const ReadinessCallback & callback)
    {
        ready_callbacks_.emplace_back(bits, inst_id, callback, clock_->currentCycle());
    }

    void ScoreboardView::clearCallbacks(const Scoreboard::InstID inst_id)
    {
        auto clear_fn = [inst_id] (auto & callbacks)
                        {
                            auto cbit = callbacks.begin();
                            const auto eit = callbacks.end();
                            while (cbit != eit)
                            {
                                const auto & cb_data = *cbit;
                                if((cb_data.inst_id == inst_id))
                                {
                                    cbit = callbacks.erase(cbit);
                                } else {
                                    ++cbit;
                                }
                            }
                        };
        clear_fn(ready_callbacks_);
    }


    Scoreboard::UnitID ScoreboardView::findMasterScoreboard_(const std::string & unit_name,
                                                             const std::string & scoreboard_type,
                                                             sparta::TreeNode * parent)
    {
        sparta_assert(parent->isFinalized(),
                      "Units should only create the views AFTER the tree if finalized. "
                      << "Consider creating the view during a startup event.");

        // Try to find the master scoreboard, if it's available (has
        // been created by the Sparta framework)

        // Go as high as the CPU node in this Tree.  If we go higher,
        // we could bind to a Scoreboard in another CPU!  That'd be
        // bad.
        auto cpu_node = parent->findAncestorByName("core*");
        sparta_assert(cpu_node != nullptr, "Could not find the core nodes in this simulation");

        std::function<Scoreboard*(sparta::TreeNode *)> findScoreboard =
            [&] (sparta::TreeNode * node) -> Scoreboard * {
                Scoreboard * scoreboard = nullptr;
                std::vector< sparta::TreeNode * > results;
                node->findChildren(scoreboard_type, results);

                if(results.size() == 0) {
                    for(auto child : node->getChildren()) {
                        scoreboard = findScoreboard(child);
                        if(scoreboard != nullptr) {
                            break;
                        }
                    }
                }
                else {
                    // There can be only one.  Or zero if not created yet...
                    sparta_assert(results.size() == 1, "Found a bunch of Scoreboards (need only 1) for type '"
                                  << scoreboard_type << "' for scoreboard '" << unit_name
                                  << "' for parent '" << parent->getLocation() << "' results: " << results);
                    try {
                        scoreboard = results[0]->getResourceAs<Scoreboard *>();
                    }
                    catch(...) {
                        const bool bad_scoreboard = false;
                        sparta_assert(bad_scoreboard, "Hmmmm... the scoreboard '"
                                      << scoreboard_type << "' isn't convertable to a Scoreboard type.  Got this: '"
                                      << results[0]->getName() << "'");
                    }
                }
                return scoreboard;
            };

        Scoreboard * master_sb = findScoreboard(cpu_node);

        // Gotta be more than 0
        sparta_assert(master_sb != nullptr,
                      "Didn't find the scoreboard '" << scoreboard_type
                      << "' for scoreboard '" << unit_name
                      << "' for parent '" << parent->getLocation() << "'");

        master_scoreboard_ = master_sb;
        return master_scoreboard_->registerView(unit_name, this);
    }

    void ScoreboardView::clearBits_(const Scoreboard::RegisterBitMask & bits)
    {
        local_ready_mask_ &= ~bits;
    }

    std::string printBitSet(const Scoreboard::RegisterBitMask & bits)
    {
        std::string ret;
        ret += '[';
        bool in_range = false;
        bool print_comma = false;
        int32_t v = 0;
        int32_t range_start_v = -1;
        while(v < int32_t(bits.size()))
        {
            if(bits.test(v))
            {
                // Determine if this bit is in the current range
                if(!in_range)
                {
                    if(print_comma) { ret += ','; }
                    ret += std::to_string(v);
                    print_comma = true;
                    // Assume within a range
                    in_range = true;
                    range_start_v = v;
                }
            }
            else {
                if(in_range) {
                    // close the range if more than one bit
                    if(range_start_v + 1 != v)
                    {
                        ret += '-';
                        ret += std::to_string(v-1);
                    }
                    in_range = false;
                }
            }
            ++v;
        }
        ret += ']';
        return ret;
    }

}
