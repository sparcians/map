// <ConfigParserYAML> -*- C++ -*-

#include <cassert>
#include <yaml-cpp/node/impl.h>
#include <yaml-cpp/node/node.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <cstddef>
#include <yaml-cpp/anchor.h>
#include <yaml-cpp/emitterstyle.h>
#include <yaml-cpp/mark.h>
#include <yaml-cpp/node/type.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stack>
#include <string>
#include <vector>

#include "sparta/parsers/ConfigParserYAML.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
#include "sparta/utils/Printing.hpp"
#include "sparta/parsers/ConfigParser.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace YP = YAML; // Prevent collision with YAML class in ConfigParser namespace.


namespace sparta
{
    namespace ConfigParser
    {

        //! Handle Scalar (key or value) YAML node from parser
        void YAML::EventHandler::OnScalar(const YP::Mark& mark, const std::string& tag, YP::anchor_t anchor, const std::string& value)
        {
            (void) anchor;
            (void) tag;

            if(subtree_.size() > 0){
                verbose() << indent_() << "(" << subtree_.size() << ") vptn:" << (pt_node_ ? pt_node_->getPath() : "<null>") << " + Scalar " << value << " @" << mark.line << std::endl;
            }else{
                verbose() << indent_() << "(commented)" << " vptn:" << (pt_node_ ? pt_node_->getPath() : "<null>") << " + Scalar " << value << " @" << mark.line << std::endl;
            }

            //if(subtree_.size() == 0){
            //    //! \todo Check to see if this is legal
            //    return;
            //}

            // New scalar within a sequence
            if(seq_params_.size() > 0){
                last_val_ = "";
                cur_ = YP::NodeType::Scalar;

                assert(sequence_pos_.size() > 0);

                // Save sequence to parameter @ subtree
                std::vector<ParameterBase*>& seq_ps = seq_params_.top();
                for(ParameterBase* pb : seq_ps){
                    verbose() << "Storing " << value << " at " << sequence_pos_
                              << " to parameter:" << *pb << std::endl;
                    if(filter_predicate_(pb)){ // Can apply?
                        if(write_to_default_){
                            pb->overrideDefaultItemValueFromString(sequence_pos_, value);
                        }else{
                            pb->setItemValueFromString(sequence_pos_, value);
                        }
                    }

                    verbose() << "Result = " << pb << std::endl;
                }

                seq_params_.addValue(value);
                //std::cerr << "seq scalar assigned @\"" << pt_node_->getPath() << "\" \"" << sequence_pos_ << "\" <- " << value << std::endl;
                //ptree_.recursPrint(std::cerr);

                sequence_pos_.back() += 1;

                return; // Done
            } else if(last_val_ != ""){
                // Value in a compact map
                verbose() << indent_() << "COMPACT MAPPING {" << last_val_ << " : " << value << "}" << std::endl;

                static const std::vector<std::string> include_keys(INCLUDE_KEYS);
                if(std::find(include_keys.begin(), include_keys.end(), last_val_) != include_keys.end()){
                    verbose() << indent_() << "  handling include directive" << std::endl;
                    handleIncludeDirective(value, subtree_, pt_node_);
                }else if(last_val_.find(COMMENT_KEY_START) == 0){
                    verbose() << indent_() << "  commented compact mapping. doing nothing" << std::endl;
                }else{
                    // Assign this value to each parameter that matches pattern
                    if(subtree_.size() > 0){
                        bool found = false;
                        std::vector<TreeNode*> nodes;
                        for(TreeNode* tn : subtree_){
                            TreeNodePrivateAttorney::findChildren(tn, last_val_, nodes);
                            for(TreeNode* n : nodes){
                                ParameterBase* pb = dynamic_cast<ParameterBase*>(n);
                                if(pb){
                                    if(filter_predicate_(pb)){ // Can apply?
                                        if(write_to_default_){
                                            pb->overrideDefaultFromString(value);
                                        }else{
                                            pb->setValueFromString(value);
                                        }
                                        found = true;
                                    }
                                }
                            }
                        }
                        if(!found && !allow_missing_nodes_){
                            std::stringstream ss;
                            ss << "Could not find at least 1 parameter node matching pattern \""
                               << last_val_ << "\" from tree nodes \"" << sparta::utils::stringize_value(subtree_)
                               << "\". Maybe the typical 'params' node was omitted from the input file "
                               << "between a node name and the actual parameter name (e.g. 'core.params.paramX')";
                            ss << markToString_(mark);
                            errors_.push_back(ss.str());
                        }
                    }

                    if(pt_node_){  // Because ptree does not handle parent references yet.
                        if(value == OPTIONAL_PARAMETER_KEYWORD){
                            const bool required = false; // Not required
                            auto n = pt_node_->create(last_val_, required);
                            n->unrequire();
                        }else{
                            const bool required = true; // Temporary value. Parameters created this way are always required
                            if(!pt_node_->set(last_val_, value, required, markToString_(mark, false))){ // Assign value
                                std::cerr << "WARNING: Encountered parameter path with parent reference: \"" << pt_node_->getPath()
                                          << "\" + \"" << last_val_ << "\". This node will not be available in the unbound parameter tree."
                                          << markToString_(mark) << std::endl;
                            }
                        }
                        //std::cerr << "set " << pt_node_->getPath() << " \"" << last_val_ << "\" <- " << value << std::endl;
                        //ptree_.recursPrint(std::cerr);
                    }
                }

                last_val_ = ""; // End of mapping pair. Clear
                cur_ = YP::NodeType::Null;
                return;
            }else if(cur_ == YP::NodeType::Null && nesting_ == 1){
                // Value in a compact map
                verbose() << indent_() << "SINGULAR SCALAR : \"" << value << "\"" << std::endl;

                if(value.find(COMMENT_KEY_START) == 0){
                    verbose() << indent_() << "  commented singular scalar. doing nothing" << std::endl;
                }else{
                    if(subtree_.size() > 0){
                        // Assign this value to each parameter that matches pattern
                        bool found = false;
                        for(TreeNode* tn : subtree_){
                            ParameterBase* pb = dynamic_cast<ParameterBase*>(tn);
                            if(pb){
                                if(filter_predicate_(pb)){ // Can apply?
                                    if(write_to_default_){
                                        pb->overrideDefaultFromString(value);
                                    }else{
                                        pb->setValueFromString(value);
                                    }
                                    found = true;
                                }
                            }
                        }
                        if(!found && !allow_missing_nodes_){
                            std::stringstream ss;
                            ss << "Could not find at least 1 parameter node in the current context \""
                               << sparta::utils::stringize_value(subtree_)
                               << "\". Maybe this YAML was parsed starting at the wrong context.";
                            ss << markToString_(mark);
                            errors_.push_back(ss.str());
                        }
                    }
                }

                if(pt_node_){  // Because ptree does not handle parent references yet.
                    if(value == OPTIONAL_PARAMETER_KEYWORD){
                        const bool required = false; // Not required
                        auto n = pt_node_->create(last_val_, required);
                        n->unrequire(); // In case it was already created
                    }else{
                        const bool required = true; // Temporary value. Parameters created this way are always required
                        pt_node_->setValue(value, required, markToString_(mark, false));
                        //std::cerr << "setValue " << pt_node_->getPath() << " \"" << last_val_ << "\" <- " << value << std::endl;
                        //ptree_.recursPrint(std::cerr);
                    }
                }

                cur_ = YP::NodeType::Null;
                return;
            }else{
                // This is legitimate. In inline-maps with comma-separated k-v pairs, this
                // case is encountered
                verbose() << indent_() << "next scalar : " << value << std::endl;
            }

            if(cur_ == YP::NodeType::Map){
                verbose() << indent_() << "<within map>" << std::endl;
            }else if(cur_ == YP::NodeType::Sequence){
                verbose() << indent_() << "<within seq>" << std::endl;
                // Append value
            }else{
                verbose() << indent_() << "<new key?>" << std::endl;
            }

            last_val_ = value;
            cur_ = YP::NodeType::Scalar;
        }

        //! Handle SequenceStart YAML node from parser
        void YAML::EventHandler::OnSequenceStart(const YP::Mark& mark, const std::string& tag,
                                                 YP::anchor_t anchor, YP::EmitterStyle::value style)
        {
            (void) anchor;
            (void) tag;
            (void) style;

            if(subtree_.size() > 0){
                verbose() << indent_() << "(" << subtree_.size() << ") vptn:" << (pt_node_ ? pt_node_->getPath() : "<null>") << " + SeqStart (" << last_val_ << ") @" << mark.line << std::endl;
            }else{
                verbose() << indent_() << "(commented)" << " vptn:" << (pt_node_ ? pt_node_->getPath() : "<null>") << " + SeqStart (" << last_val_ << ") @" << mark.line << std::endl;
            }

            //if(subtree_.size() == 0){
            //    return;
            //}

            seq_vec_.push({}); // Add a new level of values to the stack

            if(seq_params_.size() == 0){
                // Handle entry into first level of the sequence
                sequence_pos_.push_back(0);
                seq_params_.push({});
                std::vector<ParameterBase*>& seq_ps = seq_params_.top();

                if(subtree_.size() > 0){
                    // Find next generation based on pattern of scalar (key) last_val_
                    NodeVector nodes;
                    findNextGeneration_(subtree_, last_val_, nodes, mark);
                    bool found = false;
                    for(TreeNode* tn : nodes){
                        ParameterBase* pb = dynamic_cast<ParameterBase*>(tn);
                        if(pb){
                            if(filter_predicate_(pb)){ // Can apply?
                                seq_ps.push_back(pb);
                                found = true;

                                // Clear the parameter value before setting it (in case it is larger than the new content)
                                if (write_to_default_) {
                                    pb->overrideDefaultClearVectorValue();
                                } else {
                                    pb->clearVectorValue();
                                }
                            }
                        }
                    }

                    //! \todo Implement filtering with a lambda functor such as:
                    //!       [] (TreeNode* in) -> bool {return dynamic_cast<ParameterBase*>(in)));

                    if(!found && !allow_missing_nodes_){
                        std::stringstream ss;
                        ss << "Could not find at least 1 parameter node matching pattern \""
                           << last_val_ << "\" from tree node \"" << sparta::utils::stringize_value(subtree_)
                           << "\". Maybe the 'params' level was omitted from the input file's tree before the parameter";
                        ss << markToString_(mark);
                        errors_.push_back(ss.str());
                    }
                }

                pt_stack_.push(pt_node_); // Push on first entry
                if(pt_node_){
                    const bool required = true; // Temporary value. Parameters created this way are always required
                    auto npt_node = pt_node_->create(last_val_, required); // Fails if it contains a parent reference
                    //std::cerr << "OnSequenceStart Create \"" << pt_node_->getPath() << "\" \"" << last_val_ << "\"" << std::endl;
                    //ptree_.recursPrint(std::cerr);
                    if(!npt_node){
                        std::cerr << "WARNING: Encountered parameter path with parent reference: \"" << pt_node_->getPath()
                                  << "\" + \"" << last_val_ << "\". This node will not be available in the unbound parameter tree."
                                  << markToString_(mark) << std::endl;
                    }
                    pt_node_ = npt_node;
                }

            }else{
                // Enlarge the parameter at current indices before moving into the next level
                for(auto pb : seq_params_.top()){
                    if(filter_predicate_(pb)){ // Can apply?
                        if (write_to_default_){
                            pb->overrideDefaultResizeVectorsFromString(sequence_pos_);
                        } else {
                            pb->resizeVectorsFromString(sequence_pos_);
                        }
                    }
                }

                sequence_pos_.push_back(0);

                // Handle entry into nested sequence in YAML file
                seq_params_.push(seq_params_.top()); // Copy previous level
            }

            last_val_ = "";
            nesting_++;
        }

        //! Handle SequenceEnd YAML node from parser
        void YAML::EventHandler::OnSequenceEnd()
        {
            sparta_assert(seq_vec_.size() > 0,
                          "Reached end of a YAML sequence in " << filename_
                          << " without any sets of sequence values tracked");
            auto& seq_vals = seq_vec_.top();
            verbose() << indent_() << "Storing sequence to param: " << sparta::utils::stringize_value(seq_vals) << std::endl;

            if(subtree_.size() > 0){
                verbose() << indent_() << "(" << subtree_.size() << ") vptn:" << (pt_node_ ? pt_node_->getPath() : "<null>") << " + SeqEnd" << std::endl;
            }else{
                verbose() << indent_() << "(commented)" << " vptn:" << (pt_node_ ? pt_node_->getPath() : "<null>") << " + SeqEnd" << std::endl;
            }
            nesting_--;

            //if(subtree_.size() == 0){
            //    return; // seq_params_ should be NULL and seq_vec_ empty
            //}

            sparta_assert(seq_params_.size() > 0,
                          "Reached end of YAML sequence in " << filename_
                          << " without any sets of sequence parameters tracked");

            seq_params_.pop();

            // Assign popped sequence to parameter tree node once the nessted sequence has ended
            if(seq_params_.size() == 0){
                if(pt_node_){
                    // NOTE: cannot unrequire this parameter here because we're assigning a
                    // sequence to it. Only when assigning the OPTIONAL_PARAMETER_KEYWORD
                    // string can it be ignored.

                    // Just ensure a node with last_val_ exists, but do not assign to it yet.
                    const bool required = true; // Temporary value. Parameters created this way are always required
                    pt_node_->setValue(seq_params_.getValue(), required);
                    //std::cerr << "OnSequenceEnd setValue @\"" << pt_node_->getPath() << " \"" << seq_params_.getValue() << "\"" << std::endl;
                    //ptree_.recursPrint(std::cerr);
                }

                // Reached end of nested sequence, pop pt_stack_ tos to get node before sequences started
                pt_node_ = pt_stack_.top();
                pt_stack_.pop();
            }

            seq_vec_.pop();
            sequence_pos_.pop_back();
            if(!sequence_pos_.empty()){
                sequence_pos_.back() += 1;
            }

            last_val_ = "";
        }

        //! Handle MapStart YAML node from parser
        void YAML::EventHandler::OnMapStart(const YP::Mark& mark, const std::string& tag,
                                            YP::anchor_t anchor, YP::EmitterStyle::value style)
        {
            (void) anchor;
            (void) tag;
            (void) style;

            if(subtree_.size() > 0){
                verbose() << indent_() << "(" << subtree_.size() << ") vptn:" << (pt_node_ ? pt_node_->getPath() : "<null>") << " + MapStart (" << last_val_ << ") @" << mark.line << std::endl;
            }else{
                verbose() << indent_() << "(commented)" << " vptn:" << (pt_node_ ? pt_node_->getPath() : "<null>") << " + MapStart (" << last_val_ << ") @" << mark.line << std::endl;
            }
            nesting_++;

            sparta_assert(seq_params_.size() == 0,
                          "Cannot start a YAML map if already within a sequence " << markToString_(mark));

            tree_stack_.push(subtree_); // Add to stack regardless of comment-state
            pt_stack_.push(pt_node_);

            //if(subtree_.size() == 0){
            //    return; // subtree_ remains NULL
            //}

            static const std::vector<std::string> include_keys(INCLUDE_KEYS);
            if(std::find(include_keys.begin(), include_keys.end(), last_val_) != include_keys.end()){
                verbose() << indent_() << "  INCLUDE MAPPING" << std::endl;
                SpartaException ex("Include directive contains a map. This is not allowed. ");
                ex << "Includes must map directly to a filename scalar";
                addMarkInfo_(ex, mark);
                throw ex;

            }else if(last_val_.find(COMMENT_KEY_START) == 0){
                // comment
                verbose() << indent_() << "  COMMENTED MAPPING" << std::endl;
                ///subtree_
                subtree_.clear(); // Clear current nodes
            }else{
                // current subtree_ already pushed to stack
                // Move onto next generation of children
                ///subtree_ = subtree_->getChild(last_val_);
                subtree_.clear(); // Clear this level to be reset
                NodeVector& v = tree_stack_.top();
                findNextGeneration_(v, last_val_, subtree_, mark); // Assures found num nodes in range [1, MAX_MATCHES_PER_LEVEL].

                if(pt_node_){ // Because ptree cannot handle parent references yet
                    const bool required = true; // Temporary value. Parameters created this way are always required
                    auto npt_node = pt_node_->create(last_val_, required); // create child if not already existing
                    if(!npt_node){
                        std::cerr << "WARNING: Encountered parameter path with parent reference: \"" << pt_node_->getPath()
                                  << "\" + \"" << last_val_ << "\". This node will not be available in the unbound parameter tree."
                                  << markToString_(mark) << std::endl;
                    }
                    pt_node_ = npt_node;
                }
            }

            last_val_ = "";
        }

        //! Handle MapEnd YAML node from parser
        void YAML::EventHandler::OnMapEnd()
        {
            if(subtree_.size() > 0){
                verbose() << indent_() << "(" << subtree_.size() << ") vptn:" << (pt_node_ ? pt_node_->getPath() : "<null>") << " + MapEnd" << std::endl;
            }else{
                verbose() << indent_() << "(commented)" << " vptn:" << (pt_node_ ? pt_node_->getPath() : "<null>") << " + MapEnd" << std::endl;
            }
            nesting_--;
            subtree_ = tree_stack_.top();
            tree_stack_.pop();

            pt_node_ = pt_stack_.top();
            pt_stack_.pop();

            last_val_ = "";
        }


        void YAML::EventHandler::findNextGeneration_(NodeVector& current, const std::string& pattern,
                                                     NodeVector& next, const YP::Mark& mark){

            sparta_assert(next.size() == 0);

            if(current.size() == 0){
                // Breaks sparta TreeNode test
                //sparta_assert(allow_missing_nodes_);
                return;
            }

            for(TreeNode* tn : current){
                TreeNodePrivateAttorney::findChildren(tn, pattern, next);
            }
            if(next.size() == 0 && !allow_missing_nodes_){
                SpartaException ex("Could not find any nodes matching the pattern \"");
                ex << pattern << "\" from nodes " << sparta::utils::stringize_value(current) << ".";
                addMarkInfo_(ex, mark);
                throw ex;
            }
            if(next.size() > MAX_MATCHES_PER_LEVEL){
                SpartaException ex("Found more than ");
                ex << (size_t)MAX_MATCHES_PER_LEVEL << " nodes matching the pattern \""
                   << pattern << "\" from " << subtree_.size() << " nodes. "
                   << "This is likely a very deep and dangerous search pattern (or possibly a bug). "
                   << "If there really should be this many matches, increase MAX_MATCHES_PER_LEVEL.";
                addMarkInfo_(ex, mark);
                throw ex;
            }
        }

        /*!
         * \brief Sets the given sequence YAML node <node> as the value
         * of the parameter described by <param_path> relative to the
         * current node <subtree>.
         * \param subtree Current node.
         * \param param_path Path (pattern) relative to <subtree> to a
         * node which is a sparta::Parameter.
         * \param node The value to assign to the parameter decribed by
         * <subtree> and <param_path>.
         */
        void YAML::EventHandler::applyArrayParameter(TreeNode* subtree,
                                                     const std::string& param_path,
                                                     const YP::Node& node)
        {
            sparta_assert(node.Type() == YP::NodeType::Sequence);
            sparta_assert(subtree || allow_missing_nodes_);

            std::vector<TreeNode*> nodes;
            findNextGeneration_(subtree_, param_path, nodes, node.Mark());

            const bool required = true; // Temporary value. Parameters created this way are always required
            ParameterTree::Node* ptn = pt_node_ ? pt_node_->create(param_path, required) : nullptr; // create child if not already existing

            std::vector<std::string> values;
            verbose() << indent_() << "      [" << std::endl;

            // Extract each value into values vector
            for(size_t i=0; i<node.size(); ++i) {
                std::string scalar;
                scalar = node[i].Scalar();

                verbose() << indent_() << "      " << scalar << " " << std::endl;
                values.push_back(scalar);
            }
            verbose() << indent_() << "      ]" << std::endl;

            // Assign this array of values to each node
            bool found = false;
            for(TreeNode* n : nodes){
                ParameterBase* pb = dynamic_cast<ParameterBase*>(n);
                if(pb){
                    if(filter_predicate_(pb)){ // Can apply?
                        if(write_to_default_){
                            pb->overrideDefaultFromStringVector(values);
                        }else{
                            pb->setValueFromStringVector(values);
                        }
                        found = true;
                    }
                }
            }
            if(!found && !allow_missing_nodes_){
                std::stringstream ss;
                ss << "Could not find at least 1 parameter node matching pattern \""
                   << param_path << "\" from tree nodes \"" << sparta::utils::stringize_value(subtree_)
                   << "\". Maybe the typical 'params' node was omitted from the input file "
                   << "between a node name and the actual parameter name (e.g. 'core.params.paramX')";
                ss << markToString_(node.Mark());
                errors_.push_back(ss.str());
            }

            if(ptn){ // Because ptree cannot handle parent references yet
                std::stringstream ss;
                ss << values;
                ptn->setValue(ss.str(), required, markToString_(node.Mark()));
                //std::cerr << "setValue " << pt_node_->getPath() << " \"" << ss.str() << "\"" << std::endl;
                //ptree_.recursPrint(std::cerr);
            }else if(pt_node_){
                std::cerr << "WARNING: Encountered parameter path with parent reference: \"" << pt_node_->getPath()
                          << "\" + \"" << param_path << "\". This node will not be available in the unbound parameter tree."
                          << markToString_(node.Mark()) << std::endl;
            }
        }

        /*!
         * \brief Consumes a file based on an include directives destination.
         * \param pfilename YAML file to read
         * \param device_trees Vector of TreeNodes to act as roots of
         * the filename being read. This allows includes to be scoped to specific
         * nodes. The file will be parsed once an applied to all roots in
         * device_trees.
         */
        void YAML::EventHandler::handleIncludeDirective(const std::string& filename, NodeVector& device_trees, ParameterTree::Node* ptn)
        {
            //! \todo Be smarter about checking for indirect recursion by keeping a set
            //        files or limiting the depth of this include stack

            // Prevent direct reursion by filename
            if(filename == filename_){
                throw SpartaException("Direct recursion detected in configuration file. File ")
                    << filename_ << " includes " << filename;
                //! \todo Include line number in this error
            }

            std::string filename_used = filename;
            // Check to see if we can point to the relative filepath to include based on the
            // filepath of the current yaml file in the case that the file does not exist.
            boost::filesystem::path fp(filename);

            bool found = false;

            // Try to find the incldue in the include paths list
            for(const auto & incl_path : include_paths_)
            {
                boost::filesystem::path curr_inc(incl_path);
                const auto combined_path = curr_inc / filename;
                if (boost::filesystem::exists(combined_path))
                {
                    std::cout << "  [PARAMETER INCLUDE NOTE] : Including " << combined_path << std::endl;
                    filename_used = combined_path.string();
                    found = true;
                    break;
                }
            }

            if(!found) {
                SpartaException e("Could not resolve location of included file: '");
                e << filename << "' in source file: " << filename_ << "\nSearch paths: \n";
                for(const auto & incl_path : include_paths_) {
                    e << "\t" << incl_path << '\n';
                }
                e << '\n';
                throw e;
            }

            YAML incl(filename_used, include_paths_);
            incl.allowMissingNodes(doesAllowMissingNodes()); // Allow missing nodes if parent does
            incl.setParameterApplyFilter(filter_predicate_); // How to filter nodes that are about to be assigned

            TreeNode dummy("dummy", "dummy");
            NodeVector dummy_tree{&dummy};
            incl.consumeParameters(device_trees.size() > 0 ? device_trees : dummy_tree, verbose_); // Throws on error
            if(ptn){ // Because ptree cannot handle parent references yet
                ptn->appendTree(incl.getParameterTree().getRoot()); // Bring over tree
            }
        }

    }; // namespace ConfigParser

}; // namespace sparta
