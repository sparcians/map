// <ConfigEmitterYAML> -*- C++ -*-

#ifndef __CONFIG_EMITTER_YAML_H__
#define __CONFIG_EMITTER_YAML_H__

#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <vector>
#include <yaml-cpp/eventhandler.h>
#include <yaml-cpp/yaml.h>

#include "sparta/parsers/ConfigEmitter.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/app/SimulationInfo.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"

namespace YP = YAML; // Prevent collision with YAML class in ConfigEmitter namespace.

namespace sparta {
    namespace ConfigEmitter {

/*!
 * \brief Renders a TreeNode-based device tree to a file as YAML.
 * \note Opens a filestream immediately. Closes only when this class is
 * destructed.
 * \todo Use a logger
 *
 * Example:
 * \code{.cpp}
 * // Given some const TreeNode* top;
 * YAML emitter("output.yaml");
 * emitter.addParameters(top);
 * \endcode
 */
class YAML : public ConfigEmitter
{

public:

    /*!
     * \brief Constructor for a YAML parameter file emitter
     * \param filename Path of file to write. Must be writable
     * \param show_param_descs Should parameter descriptions be shown (as
     * comments in the output YAML file)?
     * \throw exception if filename cannot be opened for write
     */
    YAML(const std::string& filename, bool show_param_descs=false) :
        ConfigEmitter(filename),
        fout_(filename.c_str(), std::ios_base::out),
        filename_(filename),
        show_param_descs_(show_param_descs)
    {

        if(false == fout_.is_open()){
            throw ParameterException("Failed to open YAML Configuration file for write \"")
                << filename << "\"";
        }
        // Throw on write failure
        fout_.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);
    }

    /*!
     * \brief Destructor
     * \post Output stream will be closed
     */
    ~YAML()
    {
    }

    /*!
     * \brief Write parameters to YAML file and flushes.
     * \param device_tree Any node in a device tree to use as the root
     * yaml output. This not will not be included in the output, but
     * its immediate children and all descendants will. Must not be 0.
     * \param verbose Display verbose output messages to stdout/stderr
     * \throw exception on failure
     * \note Filestream is NOT closed after this call
     * \pre emitter_ must be nullptr
     * \post emitter_ will be nullptr
     */
    void addParameters(TreeNode* device_tree,
                       bool verbose=false, bool isPower=false)
    {
        sparta_assert(emitter_ == nullptr);
        sparta_assert(device_tree);

        if(verbose){
            std::cout << "Writing parameters to \"" << filename_ << "\"" << std::endl;
        }

        emitter_.reset(new YP::Emitter());
        emitter_->SetBoolFormat(YP::TrueFalseBool); // Render bools as true/false
        emitter_->SetSeqFormat(YP::Flow); // Render sequences in compact flow form: [a, b, c]

        // Write a header
        auto lines = SimulationInfo::getInstance().stringizeToLines("","\n");
        for(auto& line : lines){
            *emitter_ << YP::Comment(line)
                    << YP::Newline;
        }

        *emitter_ << YP::BeginDoc;
        sparta_assert(emitter_->good());
        handleNode_(device_tree, verbose, isPower); // Recurse

        if (!tree_node_extensions_.empty()) {
            for (auto & ext_info : tree_node_extensions_) {
                TreeNode * tn = ext_info.first;
                std::vector<std::pair<std::string, TreeNode::ExtensionsBase*>> & node_extensions =
                    ext_info.second;

                *emitter_ << YP::BeginMap;
                *emitter_ << YP::Key << tn->getLocation();
                *emitter_ << YP::Value;
                *emitter_ << YP::BeginMap;

                for (auto & node_extension : node_extensions) {
                    *emitter_ << YP::Key << ("extension." + node_extension.first);
                    *emitter_ << YP::Value;
                    *emitter_ << YP::BeginMap;

                    TreeNode::ExtensionsBase * ext_base = node_extension.second;
                    ParameterSet * params = ext_base->getYamlOnlyParameters();
                    auto param_names = params->getNames();
                    for (const auto & param_name : param_names) {
                        *emitter_ << YP::Key << param_name;
                        *emitter_ << YP::Value //  << YP::PadToColumn(50)
                                  << params->getParameter(param_name)->getValueAsString();
                    }

                    *emitter_ << YP::EndMap;
                }
                *emitter_ << YP::EndMap;
                *emitter_ << YP::EndMap;
            }
        }

        *emitter_ << YP::EndDoc;
        sparta_assert(emitter_->good());

        if(false == emitter_->good()){
            ParameterException ex("Error writing parameters to YAML file \"");
            ex << filename_ << "\": " << emitter_->GetLastError();
            throw ex;
        }

        fout_ << emitter_->c_str();
        fout_.flush();

        // Reset to close emitter before announcing that write is done
        emitter_.reset(nullptr);

        if(verbose){
            std::cout << "Done writing parameters to \"" << filename_ << "\"" << std::endl;
        }
   }


private:

    /*!
     * \brief Render the content of this node as a sequence of YAML
     * nodes to the current YAML emitter
     * \pre emitter_ must not be 0
     * \note Recursive. May call self following a Value node.
     *
     * YAML stream context: Must follow a BeginDoc or Value node.
     */
    void handleNode_(TreeNode* subtree, bool verbose, bool isPower)
    {
        sparta_assert(subtree);
        sparta_assert(emitter_ != nullptr);

        const auto & extension_names = subtree->getAllExtensionNames();
        for (const auto & ext_name : extension_names) {
            auto extension = subtree->getExtension(ext_name);
            if (extension) {
                tree_node_extensions_[subtree].emplace_back(
                    std::make_pair(ext_name, subtree->getExtension(ext_name)));
            }
        }

        // Print parameter value if this node is a parameter
        const ParameterBase* pb = dynamic_cast<const ParameterBase*>(subtree);
        if(pb){
            handleParameterValue_(pb, verbose);
        }else{
            // Parameters cannot have their own parameters

            // Determine if this node or its children have any parameters.
            // If not, it will be pruned
            if(subtree->getRecursiveNodeCount<ParameterBase>() == 0){
                if(verbose){
                    std::cout << "Pruned subtree with no parameters: " << subtree
                              << " while writing configuration file" << std::endl;
                }
            }else if(TreeNodePrivateAttorney::getAllChildren(subtree).size() > 0){
                // Iterate over children of this node, ignoring children with no name
                *emitter_ << YP::BeginMap;
                sparta_assert(emitter_->good());

                for(TreeNode* child : TreeNodePrivateAttorney::getAllChildren(subtree)){
                    if(verbose){
                        std::cout << "handling child " << child->getLocation() << std::endl;
                    }
                    if(child->getName().size() == 0){
                        if(verbose){
                            std::cout << "  ignoring child because it has no name" << std::endl;
                        }
                    }else{
                        const ParameterBase* cpb = dynamic_cast<const ParameterBase*>(child);
                        if(child->getRecursiveNodeCount<ParameterBase>() == 0){
                            if(verbose){
                                std::cout << "Skipping child of subtree with no parameters: " << subtree
                                          << " while writing configuration file" << std::endl;
                            }
                        }else if((cpb) && (isPower)){
                            if(cpb->hasTag("tesla_param")){
                                *emitter_ << YP::Key;
                                sparta_assert(emitter_->good());

                                checkParent(child);

                                if(show_param_descs_){
                                    // A description of the child must be printed
                                    // here if it will be a parameter
                                    if(nullptr != dynamic_cast<const ParameterBase*>(child)){
                                        //*emitter_ << YP::Newline << YP::Comment(child->getDesc());
                                        //const auto indent = emitter_->getColumn();
                                        //const auto pre_comment_indent = emitter_->GetPreCommentIndent();
                                        //emitter_->SetPreCommentIndent(0); // yaml-cpp modified to allow 0 indent
                                        *emitter_ << YP::Newline
                                            // << YP::PadToColumn(indent - pre_comment_indent)
                                                  << YP::Comment(child->getDesc())
                                                  << YP::Newline;
                                        // << YP::PadToColumn(indent);
                                        //emitter_->SetPreCommentIndent(pre_comment_indent); // yaml-cpp modified to allow 0 indent
                                    }
                                }

                                // Write child name as YAML key

                                *emitter_ << child->getName();
                                sparta_assert(emitter_->good());

                                // Write child value (or children) as YAML value
                                *emitter_ << YP::Value;
                                sparta_assert(emitter_->good());
                                handleNode_(child, verbose, isPower);

                            }
                        }else{

                            *emitter_ << YP::Key;
                            sparta_assert(emitter_->good());

                            if(show_param_descs_){
                                // A description of the child must be printed
                                // here if it will be a parameter
                                if(nullptr != dynamic_cast<const ParameterBase*>(child)){
                                    //*emitter_ << YP::Newline << YP::Comment(child->getDesc());
                                    //const auto indent = emitter_->getColumn();
                                    //const auto pre_comment_indent = emitter_->GetPreCommentIndent();
                                    //emitter_->SetPreCommentIndent(0); // yaml-cpp modified to allow 0 indent
                                    *emitter_ << YP::Newline
                                        // << YP::PadToColumn(indent - pre_comment_indent)
                                              << YP::Comment(child->getDesc())
                                              << YP::Newline;
                                    // << YP::PadToColumn(indent);
                                    //emitter_->SetPreCommentIndent(pre_comment_indent); // yaml-cpp modified to allow 0 indent
                                }
                            }

                            // Write child name as YAML key

                            *emitter_ << child->getName();
                            sparta_assert(emitter_->good());

                            // Write child value (or children) as YAML value
                            *emitter_ << YP::Value;
                            sparta_assert(emitter_->good());
                            handleNode_(child, verbose, isPower);
                        }
                    }
                }

                *emitter_ << YP::EndMap;
                sparta_assert(emitter_->good());
            }
        }

        if(false == emitter_->good()){
            if(verbose){
                std::cerr << "Emitter is BAD after handling node " << subtree->getLocation() << std::endl;
            }
        }
    }

    /*!
     * \brief Renders a parameter value (whether scalar or sequence of
     * scalars) to the current YAML emitter
     *
     * YAML stream context: Must follow a parameter node
     */
    void handleParameterValue_(const ParameterBase* p, bool verbose)
    {
        //*emitter_ << YP::PadToColumn(50);

        //if(p->isVector()){
        //    if(verbose){
        //        std::cout << "handling parameter (vector) " << p->getName() << std::endl;
        //    }
        //    *emitter_ << YP::BeginSeq;
        //    sparta_assert(emitter_->good());
        //    for(ParameterBase::const_iterator it=p->begin(); it != p->end(); ++it){
        //        *emitter_ << *it;
        //    }
        //    *emitter_ << YP::EndSeq;
        //    sparta_assert(emitter_->good());
        //}else{
        //    if(verbose){
        //        std::cout << "handling parameter " << p->getName() << std::endl;
        //    }
        //    *emitter_ << p->getValueAsString();
        //    sparta_assert(emitter_->good());
        //}

        std::vector<uint32_t> indices;
        writeParameterValue_(p, indices, verbose);

        // yaml-cpp has been modified so that if comments begin with a
        // \t, they are aligned to a column with consistent indentation
        // (unless cursor is already past that column)
        std::stringstream comment;
        if(p->isDefault()){
            // Do not change this string. It will break scripts
            comment << "\tdefault (" << p->getTypeName() << ")";
        }else{
            // Do not change this string. It will break scripts
            comment << "\tNON-DEFAULT: " << p->getDefaultAsString() << " (" << p->getTypeName() << ")";
        }
        *emitter_ << YP::Comment(comment.str());
    }

    void writeParameterValue_(const ParameterBase* p,
                              std::vector<uint32_t>& indices,
                              bool verbose)
    {
        // Handle writing non-vector value
        // Implicitly handles p->getDimensionality() == 0
        if(indices.size() == p->getDimensionality()){
            if(verbose){
                std::cout << "handling parameter item at " << p->getName()
                          << " " << indices << std::endl;
            }
            *emitter_ << p->peekItemValueFromString(indices);
            sparta_assert(emitter_->good());
            return;
        }

        // Iterate through all elements in the vector identified by indices
        *emitter_ << YP::BeginSeq;
        sparta_assert(emitter_->good());

        const uint32_t size = p->peekVectorSizeAt(indices);

        indices.push_back(0);
        for(uint32_t j = 0; j < size; ++j){
            writeParameterValue_(p, indices, verbose);
            indices.back() += 1;
        }
        indices.pop_back();

        *emitter_ << YP::EndSeq;
        sparta_assert(emitter_->good());
    }

    // Checks if each of the parent nodes except root has the power_entity tag
    void checkParent(const TreeNode* child)
    {
        const TreeNode* parent = child->getParent();
        sparta_assert(parent);
        if(parent->getParent()){
            if(parent->hasTag("power_entity")){
                checkParent(parent); // Recurse back
            }else{
                 throw SpartaException("Ancestor not tagged as power_entity ")
                    << parent << "whose location is " << parent->getLocation();
            }
        }else{
            return;
        }

    }

    /*!
     * \brief Output file stream
     */
    std::ofstream fout_;

    /*!
     * \brief YP::Emitter (YAML event output stream)
     */
    std::unique_ptr<YP::Emitter> emitter_;

    /*!
     * \brief Filename of output for displaying error information
     */
    std::string filename_;

    /*!
     * \brief Should the the parameter descriptions be shown (as comments)
     */
    const bool show_param_descs_;

    /*!
     * \brief Mapping from tree nodes to their named extensions, if any
     */
    std::unordered_map<TreeNode*,
        std::vector<std::pair<std::string, TreeNode::ExtensionsBase*>>> tree_node_extensions_;

}; // class YAML

    }; // namespace ConfigEmitter
}; // namespace sparta

// __CONFIG_EMITTER_YAML__
#endif
