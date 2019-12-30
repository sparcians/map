// <YAMLTreeListener> -*- C++ -*-


/*!
 * \file Tree event handler classs for yaml-cpp that maintains a stack of a tree
 * \brief
 */

#include "sparta/parsers/YAMLTreeEventHandler.hpp"

#include "sparta/parsers/ConfigParser.hpp"

namespace sparta
{

    //! Handle Scalar (key or value) YAML node from parser
    void YAMLTreeEventHandler::OnScalar(const YP::Mark& mark,
                                        const std::string& tag,
                                        YP::anchor_t anchor,
                                        const std::string& value)
    {
        (void) anchor;
        (void) tag;

        if(subtree_.size() > 0){
            verbose() << indent_() << "(ctxt size=" << subtree_.size() << ") + Scalar " << value << " @line "
                      << mark.line << std::endl;
        }else{
            verbose() << indent_() << "(commented)" << " + Scalar " << value << " @line " << mark.line
                      << std::endl;
        }

        // This entire subtree was commented out
        if(subtree_.size() == 0){
            return;
        }

        // New scalar within a sequence
        if(in_sequence_){
            last_val_ = "";
            cur_ = YP::NodeType::Scalar;
            seq_vec_.push_back(value);
            return; // Done
        }

        if(last_val_ != ""){
            // Compact map
            verbose() << indent_() << "COMPACT MAPPING {" << last_val_ << " : " << value << "}"
                      << std::endl;

            static const std::vector<std::string> include_keys(INCLUDE_KEYS);
            if(std::find(include_keys.begin(), include_keys.end(), last_val_) != include_keys.end()){
                verbose() << indent_() << "  handling include directive" << std::endl;
                handleIncludeDirective_(value, subtree_);
            }else if(last_val_.find(COMMENT_KEY_START) == 0){
                verbose() << indent_() << "  commented compact mapping. doing nothing" << std::endl;
            }else if(!traverseSequence_()) {
                verbose() << indent_() << "  told to ignore the sequence/scalar" << std::endl;
            }else{
                // Key (last_val_) is the relative location pattern of a
                // TreeNode or a reserved key.
                // value is the thing to associate with that node
                // Handle this relationship for each leaf that matches
                // pattern in whatever way the subclass wants

                if(isReservedKey_(last_val_)){

                    verbose() << indent_() << "Handling leaf scalar " << last_val_ << " for " << subtree_ << std::endl;

                    // One call to this method with full context available able
                    handleLeafScalarContexts_(value, last_val_, subtree_);

                    // Iterate through contexts and filter out nodes, then call
                    // handleLeafScalar_ as a convenience to the subclass so it
                    // doesn't need to do this in handleLeafScalarContexts_
                    for(auto& nvp : subtree_){
                        //std::cerr << "Leaf Scalar: ";
                        //nvp->dump(std::cerr);
                        //std::cerr << std::endl;
                        if(acceptNode(nvp->first)){
                            handleLeafScalar_(nvp->first,
                                              value,
                                              last_val_,
                                              nvp->second, // Captures
                                              nvp->uid);
                        }
                    }
                }else{
                    // Handle an un-reserved key. It might be a node or something else
                    bool found = false;

                    // Get all children and all replacements
                    for(auto& nvp : subtree_){
                        NodeVector nodes;
                        std::vector<std::vector<std::string>> replacements;
                        nvp->first->findChildren(last_val_, nodes, replacements);

                        // Look at all the children found
                        size_t idx = 0;
                        for(TreeNode* n : nodes){
                            if(acceptNode(nvp->first)){
                                found = true;
                                std::vector<std::string> all_replacements(nvp->second);
                                std::vector<std::string>& added = replacements.at(idx);
                                all_replacements.resize(all_replacements.size() + added.size());
                                std::copy(added.begin(), added.end(), all_replacements.rbegin());
                                handleLeafScalar_(n,
                                                  value,
                                                  last_val_,
                                                  all_replacements, // Captures
                                                  nvp->uid);
                            }
                            ++idx;
                        }
                    }
                    if(!found){
                        // No children found, so this might be something else (e.g. stat expression)
                        for(auto& nvp : subtree_){
                            auto& n = nvp->first;
                            //auto& replacements = nvp->second;
                            if(!handleLeafScalarUnknownKey_(n, value, last_val_, *nvp)){
                                std::stringstream ss;
                                ss << "\tError found while parsing YAML file: " << markToString_(mark);
                                addError_(ss.str());
                            }
                            // Keep the parser going, looking for more errors
                            found = true;
                        }
                    }
                    if(!found){
                        std::stringstream ss;
                        ss << "Could not find at least 1 node matching pattern \""
                           << last_val_ << "\" from tree nodes \"" << sparta::utils::stringize_value(subtree_)
                           << "\". Maybe the typical container node (e.g. params, stats) was omitted "
                           "from the input file between a node name and the actual leaf node name "
                           "(e.g. 'core.params.paramX'). ";
                        ss << markToString_(mark);
                        addError_(ss.str());
                    }
                }
            }

            last_val_ = ""; // End of mapping pair. Clear
            cur_ = YP::NodeType::Null;
            return;
        }

        last_val_ = value;
        cur_ = YP::NodeType::Scalar;
    }

    //! Handle SequenceStart YAML node from parser
    void YAMLTreeEventHandler::OnSequenceStart(const YP::Mark& mark, const std::string& tag,
                                               YP::anchor_t anchor, YP::EmitterStyle::value style)
    {
        (void) anchor;
        (void) tag;
        (void) style;

        if(subtree_.size() > 0){
            verbose() << indent_() << "(" << subtree_.size() << ") + SeqStart (" << last_val_
                      << ") @" << mark.line << std::endl;
        }else{
            verbose() << indent_() << "(commented)" << " + SeqStart (" << last_val_ << ") @"
                      << mark.line << std::endl;
        }

        // Protect from nested sequences
        sparta_assert(!in_sequence_, "No support for nested sequences in YAML file interpretation. No sparta ""configuration files require this"); // Cannot have nested sequences
        sparta_assert(seq_nodes_.size() == 0, "YAML interpreter appears to have entered a sequence without exiting ""the last sequence");

        if(subtree_.size() == 0){
            return; // No nodes on which to search - subtree_ remains NULL
        }

        // Some sequences are embedded in an entire tree that is to be ignored or skipped
        if(traverseSequence_())
        {
            // Attempt to find all of the TreeNodes starting from subtree_
            // to using last_val_ as a relative path.
            NavVector nodes;
            bool found = false;
            try {
                findNextGeneration_(subtree_, last_val_, nodes, mark);
                for(auto& nvp : nodes){
                    if(acceptNode(nvp->first)){
                        found = 1;
                        seq_nodes_.push_back(nvp->first);
                    }
                }
            } catch (...) {
                sparta_assert(last_val_ == "content");
                return;
            }

            if(!found){
                std::stringstream ss;
                ss << "Could not find at least 1 node matching pattern and passing the filter \""
               << last_val_ << "\" from tree node \"" << sparta::utils::stringize_value(subtree_)
                   << "\". Maybe the typical container node (e.g. params, stats) was omitted from "
                    "the input file's tree before the node name (e.g. 'core.params.paramX'). ";
                ss << markToString_(mark);
                addError_(ss.str());
            }
            in_sequence_ = true;
        }

        last_val_ = "";
        nesting_++;
    }

    //! Handle SequenceEnd YAML node from parser
    void YAMLTreeEventHandler::OnSequenceEnd()
    {
        verbose() << indent_() << "Storing sequence to leaf node: "
                  << sparta::utils::stringize_value(seq_vec_) << std::endl;

        if(subtree_.size() > 0){
            ///verbose() << indent_() << subtree_->getLocation() << " + SeqEnd" << std::endl;
            verbose() << indent_() << "(" << subtree_.size() << ") + SeqEnd" << std::endl;
        }else{
            verbose() << indent_() << "(commented)" << " + SeqEnd" << std::endl;
        }
        nesting_--;

        if(subtree_.size() == 0){
            // Ensure that nothing happened when parsing the content of this
            // sequence
            sparta_assert(seq_nodes_.size() == 0);
            sparta_assert(seq_vec_.size() == 0);
            return; // seq_nodes_ should be NULL and seq_vec_ empty
        }

        // Save sequence to parameter @ subtree
        for(TreeNode* n : seq_nodes_){
            for(auto& nvp : subtree_){
                if(acceptNode(nvp->first)){
                    handleLeafSequence_(n, seq_vec_, last_val_, *nvp);
                }
            }
        }
        in_sequence_ = false; // No longer in a sequence
        seq_nodes_.clear(); // No nodes for this sequence
        seq_vec_.clear(); // No items in the sequence

        last_val_ = "";
    }

    //! Handle MapStart YAML node from parser
    void YAMLTreeEventHandler::OnMapStart(const YP::Mark& mark, const std::string& tag,
                                          YP::anchor_t anchor, YP::EmitterStyle::value style)
    {
        (void) anchor;
        (void) tag;
        (void) style;

        if(subtree_.size() > 0){
            verbose() << indent_() << "(" << subtree_.size() << ") + MapStart (" << last_val_
                      << ") @" << mark.line << std::endl;
        }else{
            verbose() << indent_() << "(commented)" << " + MapStart (" << last_val_ << ") @"
                      << mark.line << std::endl;
        }

        // Protect from maps within sequences
        sparta_assert(!in_sequence_, "No support for maps within sequences in YAML file interpretation. No ""sparta configuration files require this"); // Cannot have maps within sequences
        sparta_assert(seq_nodes_.size() == 0, "YAML interpreter appears to have entered a map without exiting the ""last sequence");

        // When entering a map, the previou scalar (stored in last_val_)
        // in the parent mapping for which this new map is a value. This
        // will probably be followed by a scalar representing the first key
        // inside this map.

        nesting_++;

        // Store key associated with this map in the parent map
        map_entry_key_stack_.push(last_val_);

        // If this map is a keyword, do something different
        if(!handleEnterMap_(last_val_, subtree_)){
            // Handle this reserved key instead of traversing the device tree
            verbose() << indent_() << "entered specially-handled mapping on \"" << last_val_
                      << "\"" << std::endl;

            // Carry same subtree_ on to next level updating each node to get new user-IDs
            tree_stack_.push(subtree_);
            subtree_.clear();
            inheritNextGeneration_(tree_stack_.top(), subtree_);
        }else{
            // Handle device-tree navigation recursion
            tree_stack_.push(subtree_); // Add to stack regardless of comment-state

            // current subtree_ already pushed to stack
            // Move onto next generation of children
            subtree_.clear(); // Clear this level to be reset

            if(tree_stack_.top().size() == 0){
                return; // No nodes on which to search - subtree_ remains NULL
            }

            static const std::vector<std::string> include_keys(INCLUDE_KEYS);
            if(std::find(include_keys.begin(), include_keys.end(), last_val_) != include_keys.end()){
                verbose() << indent_() << "  INCLUDE MAPPING" << std::endl;
                SpartaException ex("Include directive contains a map. This is not allowed. ");
                ex << "Includes must map directly to a filename scalar. ";
                addMarkInfo_(ex, mark);
                throw ex;

            }else if(last_val_.find(COMMENT_KEY_START) == 0){
                // comment
                verbose() << indent_() << "  COMMENTED MAPPING" << std::endl;
            }else{
                NavVector& v = tree_stack_.top();
                // Assures found num nodes in range [1, MAX_MATCHES_PER_LEVEL].
                findNextGeneration_(v, last_val_, subtree_, mark);
            }
        }

        last_val_ = "";
    }

    //! Handle MapEnd YAML node from parser
    void YAMLTreeEventHandler::OnMapEnd()
    {
        if(subtree_.size() > 0){
            verbose() << indent_() << "(" << subtree_.size() << ") + MapEnd" << std::endl;
        }else{
            verbose() << indent_() << "(commented)" << " + MapEnd" << std::endl;
        }

        // Exiting a map typically involves popping the tree stack and
        // restoring the set of subtree_. Unless this map was a special
        // entity (value of a key in parent map with special semantics)

        nesting_--;

        // Get the key associated with this map in the parent map and pop it
        sparta_assert(map_entry_key_stack_.size() > 0);
        std::string map_entry_key = map_entry_key_stack_.top();
        map_entry_key_stack_.pop();

        if(!handleExitMap_(map_entry_key, subtree_)){
            // Handle returning from a reserved keyword recursion
            verbose() << indent_() << "exiting special mapping on \"" << map_entry_key
                      << "\"" << std::endl;
            subtree_ = tree_stack_.top(); // Anything lost in subtree_ is cleaned by shared_ptr
            tree_stack_.pop();
        }else{
            // Handle returning from device-tree recursion
            sparta_assert(tree_stack_.size() > 0);
            subtree_ = tree_stack_.top(); // Anything lost in subtree_ is cleaned by shared_ptr
            tree_stack_.pop();

            last_val_ = "";
        }
    }
}; // namespace sparta
