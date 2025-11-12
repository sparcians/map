// <PersistentFastCheckpointer> -*- C++ -*-

#pragma once

#include <iostream>
#include <sstream>
#include <stack>
#include <queue>
#include <string>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/serialization/checkpoint/FastCheckpointer.hpp"

namespace sparta::serialization::checkpoint
{
    /*!
     * \brief Implements a Persistent FastCheckpointer,
     * i.e. implements an interface to save checkpoints to disk
     *
     * Used in conjunction with the fast checkpointer (which saves
     * checkpoints to memory), this class enables user the save the
     * checkpoints to disk for loading later.
     */
    class PersistentFastCheckpointer : public FastCheckpointer
    {
    public:

        /*!
         * \brief file storage adpater for ArchData
         * \see DeltaCheckpoint
         */
        class FileWriteAdapter
        {
            std::ostream& fs_;

        public:
            FileWriteAdapter(std::ostream& out) :
                fs_(out)
            {;}

            void dump(std::ostream& o) const {
                o << "<dump not supported on checkpoint file storage adapter>";
            }

            uint32_t getSize() const {
                // Return MEMORY size. Assume file is on disk.
                return sizeof(decltype(*this));
            }

            void beginLine(ArchData::line_idx_type idx) {
                fs_ << 'L'; // Line start char

                ArchData::line_idx_type idx_repr = reorder<ArchData::line_idx_type, LE>(idx);
                fs_.write((char*)&idx_repr, sizeof(ArchData::line_idx_type));
            }

            void writeLineBytes(const char* data, size_t size) {
                fs_.write(data, size);
            }

            void endArchData() {
                fs_ << "E"; // Indicates end of this checkpoint data

                sparta_assert(fs_.good(),
                              "Ostream error while writing checkpoint data");
            }

            bool good() const {
                return fs_.good();
            }
        };

        /*!
         * \brief file storage adpater for ArchData
         * \see DeltaCheckpoint
         */
        class FileReadAdapter
        {
            std::istream& fs_;

        public:
            FileReadAdapter(std::istream& in) :
                fs_(in)
            {;}

            void dump(std::ostream& o) const {
                o << "<dump not supported on checkpoint file storage adapter>";
            }

            uint32_t getSize() const {
                // Return MEMORY size. Assume file is on disk.
                return sizeof(decltype(*this));
            }

            void prepareForLoad() {
                fs_.seekg(0); // Seek to start with get pointer before consuming
            }

            bool good() const {
                return fs_.good();
            }

            ArchData::line_idx_type getNextRestoreLine() {
                char ctrl;
                fs_ >> ctrl;
                sparta_assert(fs_.good(),
                              "Encountered checkpoint data stream error or eof");
                if(ctrl == 'L'){
                    ArchData::line_idx_type ln_idx = 0;
                    fs_.read((char*)&ln_idx, sizeof(ln_idx)); // Presumed LE encoding
                    return ln_idx;
                }else if(ctrl == 'E'){
                    return ArchData::INVALID_LINE_IDX; // Done with restore
                }else{
                    throw SpartaException("Failed to restore a checkpoint because a '")
                        << ctrl << "' control character was found where an 'L' or 'E' was found";
                }
            };

            void copyLineBytes(char* buf, uint32_t size) {
                fs_.read(buf, size);
            }
        };

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief PersistentFastCheckpointer Constructor
         *
         * \param root TreeNode at which checkpoints will be taken.
         *             This cannot be changed later. This does not
         *             necessarily need to be a RootTreeNode. Before
         *             the first checkpoint is taken, this node must
         *             be finalized (see
         *             sparta::TreeNode::isFinalized). At this point,
         *             the node does not need to be finalized
         *
         * \param sched Scheduler whose current cycle will be read
         *              when taking checkpoints and restored when
         *              restoring checkpoints. See
         *              sparta::serialization::Checkpoint::Checkpoint
         *              for details
         */
        PersistentFastCheckpointer(TreeNode& root,
                                   sparta::Scheduler* sched=nullptr) :
            FastCheckpointer(root, sched),
            prefix_("chkpt"),
            suffix_("data")
        { }

        /*!
         * \brief PersistentFastCheckpointer Constructor
         *
         * \param roots TreeNodes at which checkpoints will be taken.
         *              This cannot be changed later. These do not
         *              necessarily need to be RootTreeNodes. Before
         *              the first checkpoint is taken, these nodes must
         *              be finalized (see
         *              sparta::TreeNode::isFinalized). At this point,
         *              the nodes do not need to be finalized
         *
         * \param sched Scheduler whose current cycle will be read
         *              when taking checkpoints and restored when
         *              restoring checkpoints. See
         *              sparta::serialization::Checkpoint::Checkpoint
         *              for details
         */
        PersistentFastCheckpointer(const std::vector<sparta::TreeNode*>& roots,
                                   sparta::Scheduler* sched=nullptr) :
            FastCheckpointer(roots, sched),
            prefix_("chkpt"),
            suffix_("data")
        { }

        /*!
         * \brief Destructor
         *
         */
        virtual ~PersistentFastCheckpointer() {

        }

        /*! Save checkpoint to ostream.
         *
         * \param outf Output stream to save to
         *
         * \return The checkpoint ID
         */
        FastCheckpointer::chkpt_id_t save(std::ostream& outf) {
            FastCheckpointer::chkpt_id_t checkpoint_id = createCheckpoint(true);
            save_(outf);
            return checkpoint_id;
        }

        /*! Save checkpoint to specified file.
         *
         * \param filename Filename to use for checkpoint
         *
         * \return The checkpoint ID
         */
        FastCheckpointer::chkpt_id_t save(std::string filename) {
            FastCheckpointer::chkpt_id_t checkpoint_id = createCheckpoint(true);
            std::ofstream outf(filename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
            save_(outf);
            outf.close();
            return checkpoint_id;
        }

        /*! Save checkpoint to calculated filename.
         *
         * Calculates the filename based on the configured prefix
         * and suffix, as well as the checkpoint ID.
         *
         * \return The checkpoint ID
         */
        FastCheckpointer::chkpt_id_t save() {
            const bool force_snapshot = true;
            FastCheckpointer::chkpt_id_t checkpoint_id = createCheckpoint(force_snapshot);
            std::ostringstream chkpt_filename;
            chkpt_filename << prefix_ << "."
                           << checkpoint_id
                           << "." << suffix_;
            std::ofstream outf(chkpt_filename.str(), std::ofstream::binary | std::ofstream::trunc);
            save_(outf);
            outf.close();
            return checkpoint_id;
        }

        /*! Restore checkpoint from istream.
         *
         * \param in Input stream from which to retrieve checkpoint data
         */
        void restore(std::istream& in) {
            auto adatas = getArchDatas();
            FileReadAdapter fsa(in);
            for (auto aditr=adatas.begin(); aditr!= adatas.end(); aditr++) {
                (*aditr)->restoreAll(fsa);
            }
        }

        /*! Restore checkpoint from file.
         *
         * \param filename The name of the checkpoint file
         */
        void restore(const std::string& filename) {
            std::ifstream in(filename);
            restore(in);
            in.close();
        }

    private:

        /*! Common save routine.
         *
         * \param outf Output stream to save to
         */
        void save_(std::ostream& outf) {
            // Throw on write failure
            outf.exceptions(std::ostream::eofbit | std::ostream::badbit |
                            std::ostream::failbit | std::ostream::goodbit);
            FileWriteAdapter fsa(outf);
            auto adatas = getArchDatas();
            for (auto aditr=adatas.begin(); aditr!= adatas.end(); aditr++) {
                (*aditr)->saveAll(fsa);
            }
        }

        std::string prefix_;
        std::string suffix_;

    };

} // namespace sparta::serialization::checkpoint
