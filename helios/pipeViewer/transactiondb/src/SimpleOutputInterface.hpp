#pragma once

#include "sparta/pipeViewer/Outputter.hpp"
#include "sparta/pipeViewer/transaction_structures.hpp"

namespace sparta{
    namespace pipeViewer{

        class SimpleOutputInterface {
        public:
            static constexpr uint64_t BAD_DISPLAY_ID = 0x1000;  // anything higher than 0xfff is out of bounds

            SimpleOutputInterface(const std::string& filepath, const uint64_t interval, bool debug = false) :
            outputter_(filepath, interval)
            {
                debug_ = debug;
                if(SPARTA_EXPECT_FALSE(debug_))
                {
                    std::cerr << "constructed output wrapper" << std::endl;
                }
                // default intialize annot_struct.
                annot_struct_.time_Start = 0;
                annot_struct_.time_End = 0;
                annot_struct_.parent_ID = 0;
                annot_struct_.control_Process_ID = 0;
                annot_struct_.transaction_ID = 0;
                annot_struct_.display_ID = BAD_DISPLAY_ID;
                annot_struct_.location_ID = 0;
                annot_struct_.flags = is_Annotation;
                annot_struct_.length = 0;
                annot_struct_.annt = nullptr;
            }

            void writeTransaction(uint64_t start,
                                                    uint64_t end,
                                                    uint64_t location_id,
                                                    const std::string& dat,
                                                    bool continue_transaction = false)
            {
                std::cout << "SimplePutputterInterface" << std::endl;
                annot_struct_.length = (uint16_t)(dat.size() + 1);
                if(SPARTA_EXPECT_FALSE(debug_))
                {
                    std::cerr << " annotation length = " << annot_struct_.length << std::endl;
                }
                annot_struct_.annt = dat.c_str();
                annot_struct_.time_Start = start;
                annot_struct_.time_End = end;
                annot_struct_.transaction_ID = next_transaction_id_++;
                annot_struct_.location_ID = location_id;

                if(continue_transaction)
                {
                    annot_struct_.flags |= CONTINUE_FLAG;
                }

                assert(start != end);
                if(SPARTA_EXPECT_FALSE(debug_))
                {
                    std::cerr << " ----> wrote transaction: (" << start << ", "<< end << ") " << dat << std::endl;
                }
                outputter_.writeTransaction(annot_struct_);

                if(continue_transaction)
                {
                    annot_struct_.flags &= ~CONTINUE_FLAG;
                }
            }

            void writeIndex() {
                outputter_.writeIndex();
            }

        private:
            Outputter outputter_;
            annotation_t annot_struct_;
            uint64_t next_transaction_id_ = 0;
            bool debug_;
        };
    }  // namespace pipeViewer
} // namespace sparta


