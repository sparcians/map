// <Expression> -*- C++ -*-

/*!
 * \file TreeFilterExpression.cpp
 * \brief Expression for representing a filtering function for TreeNodes based
 * on their attributes
 */

#include <regex>
#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <sstream>

#include "sparta/tree/filter/Expression.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/simulation/TreeNode.hpp"

namespace sparta {
namespace tree {
namespace filter {

    bool Expression::valid(const TreeNode* n, bool trace) const {
        sparta_assert(n,
                    "cannot evaluate valid() in filter expression with a null TreeNode");

        // Execute operation
        bool v1;
        bool v2;
        bool result;
        if(trace){
            dump(std::cout);
            std::cout << " => ";
            std::cout << std::boolalpha;
        }

        switch(op_){
        case OP_INVALID:
            throw SpartaException("Tree filter grammar expression node has OP_INVALID operation. "
                                "Cannot evaluate valid()");
        case OP_FALSE:
            sparta_assert(operands_.size() == 0,
                        "Expression Node had the wrong number of operands ("
                        << operands_.size() << ")for OP_FALSE. Expected 0");
            if(trace){
                std::cout << "FALSE => " << false << "\n";
            }
            return false;
        case OP_TRUE:
            sparta_assert(operands_.size() == 0,
                        "Expression Node had the wrong number of operands ("
                        << operands_.size() << ")for OP_TRUE. Expected 0");
            if(trace){
                std::cout << "TRUE => " << true << "\n";
            }
            return true;
        case OP_XOR:
            sparta_assert(operands_.size() == 2,
                        "Expression Node had the wrong number of operands ("
                        << operands_.size() << ")for OP_XOR. Expected 2");
            v1 = operands_.at(0).valid(n, trace);
            v2 = operands_.at(1).valid(n, trace);
            result = (v1 != v2);
            if(trace){
                std::cout << v1 << " xor " << v2 << " => " << result << "\n";
            }
            return result;
        case OP_OR:
            sparta_assert(operands_.size() == 2,
                        "Expression Node had the wrong number of operands ("
                        << operands_.size() << ")for OP_OR. Expected 2");
            v1 = operands_.at(0).valid(n, trace);
            v2 = operands_.at(1).valid(n, trace);
            result = v1 || v2;
            if(trace){
                std::cout << v1 << " or " << v2 << " => " << result << "\n";
            }
            return result;
        case OP_AND:
            sparta_assert(operands_.size() == 2,
                        "Expression Node had the wrong number of operands ("
                        << operands_.size() << ")for OP_AND. Expected 2");
            v1 = operands_.at(0).valid(n, trace);
            v2 = operands_.at(1).valid(n, trace);
            result = v1 && v2;
            if(trace){
                std::cout << v1 << " and " << v2 << " => " << result << "\n";
            }
            return result;
        case OP_NOT:
            sparta_assert(operands_.size() == 1,
                        "Expression Node had the wrong number of operands ("
                        << operands_.size() << ")for OP_NOT. Expected 1");
            v1 = operands_.at(0).valid(n, trace);
            result = !v1;
            if(trace){
                std::cout << "not " << v1 << " => " << result << "\n";
            }
            return result;
        case OP_EVAL_VIS:
            if(trace){
                std::cout << "vis " << vis_comparison_ << " " << visibility_ << " => ";
            }
            result = evaluateVisibility_(n, trace);
            if(trace){
                std::cout << " => " << result << "\n";
            }
            return result;
        case OP_EVAL_TYPE:
            result = evaluateType_(n, trace);
            if(trace){
                std::cout << " => " << result << "\n";
            }
            return result;
        case OP_EVAL_TAG:
            result = evaluateTag_(n, trace);
            if(trace){
                std::cout << " => " << result << "\n";
            }
            return result;
        case OP_EVAL_NAME:
            result = evaluateName_(n, trace);
            if(trace){
                std::cout << " => " << result << "\n";
            }
            return result;
        default:
            throw SpartaException("Tree filter grammar expression node has an unhandled operation: ")
                << op_;
        }
    }

    bool Expression::evaluateVisibility_(const TreeNode* n, bool trace) const {
        sparta_assert(n,
                    "cannot evaluate node visibility in filter expression with a null "
                    "TreeNode");

        const auto dn = dynamic_cast<const InstrumentationNode*>(n);
        if(!dn){
            if(trace){
                std::cout << n->getLocation() << " is not instrumentation";
            }
            return false;
        }
        auto dnvis = dn->getVisibility();
        if(trace){
            std::cout << "visibility of " << n->getLocation() << " is " << dnvis;
        }
        switch(vis_comparison_){
        case VISCOMP_EQ:
            return dnvis == visibility_;
        case VISCOMP_GT:
            return dnvis > visibility_;
        case VISCOMP_LT:
            return dnvis < visibility_;
        case VISCOMP_GE:
            return dnvis >= visibility_;
        case VISCOMP_LE:
            return dnvis <= visibility_;
        case VISCOMP_NE:
            return dnvis != visibility_;
        default:
            throw SpartaException("Unhandled visibility comparision value ") << vis_comparison_
                                                                           << " in tree filter expression";
        };

        return true;
    }


    bool Expression::evaluateType_(const TreeNode* n, bool trace) const {
        sparta_assert(n,
                    "cannot evaluate node type in filter expression with a null TreeNode");

        const auto dn = dynamic_cast<const InstrumentationNode*>(n);
        if(!dn){
            if(trace){
                std::cout << n->getLocation() << " is not instrumentation";
            }
            return false;
        }

        auto dntype = dn->getInstrumentationType();
        if(trace){
            std::cout << "type of " << n->getLocation() << " is " << dntype;
        }
        switch(type_comparison_){
        case TYPECOMP_EQ:
            return dntype == instrument_type_;
        case TYPECOMP_NE:
            return dntype != instrument_type_;
        default:
            throw SpartaException("Unhandled type comparision value ") << type_comparison_
                                                                     << " in tree filter expression";
        };

        return true;
    }

    bool Expression::evaluateTag_(const TreeNode* n, bool trace) const {
        sparta_assert(n,
                    "cannot evaluate node tag in filter expression with a null TreeNode");

        if(trace){
            std::cout << "tags of " << n->getLocation() << " are [";
            bool first = true;
            for(const std::string * tag :  n->getTags()){
                if(!first){
                    std::cout << ",";
                }else{
                    first = false;
                }
                std::cout << *tag;
            }
            std::cout << "]";
        }

        std::regex expr(tag_); // Create the expression regardles of comparison type
        std::smatch what;

        // Look at each tag
        for(const std::string * tag :  n->getTags()){
            switch(tag_comparison_){
            case TAGCOMP_EQ:
                if(*tag == tag_){
                    return true; // Succeeded
                }
                break;
            case TAGCOMP_NE:
                if(*tag == tag_){
                    return false; // Failed
                }
                break;
            case TAGCOMP_REM:
                if(std::regex_match(*tag, what, expr)) { // , boost::match_extra)){
                    // Print out matches
                    // Skip 0 because it is the whole expression.  match_extra might cause this
                    // These replacements will probably be concatenated together.
                    //for(unsigned i = 1; i < what.size(); ++i){
                    //    std::cout << "      $" << i << " = \"" << what[i] << "\"\n";
                    //}
                    return true; // Succeeded
                }
                break;
            default:
                throw SpartaException("Unhandled tag comparision value ") << tag_comparison_
                                                                        << " in tree filter expression";
            };
        }

        switch(tag_comparison_){
        case TAGCOMP_EQ:
            return false; // No matches = failure
        case TAGCOMP_NE:
            return true; // No matches = success
        case TAGCOMP_REM:
            return false; // No matches = failure
        default:
            throw SpartaException("Unhandled tag comparision value ") << tag_comparison_
                                                                    << " in tree filter expression";
        };
    }

    bool Expression::evaluateName_(const TreeNode* n, bool trace) const {
        sparta_assert(n,
                    "cannot evaluate node name in filter expression with a null TreeNode");

        if(trace){
            std::cout << "name of " << n->getLocation() << " is \"" << n->getName() << "\"";
        }

        std::regex expr(name_); // Create the expression regardles of comparison type
        std::smatch what;

        // Look at name
        switch(name_comparison_){
        case NAMECOMP_EQ:
            return n->getName() == name_;
        case NAMECOMP_NE:
            return n->getName() != name_;
        case NAMECOMP_REM:
            if(std::regex_match(n->getName(), what, expr)) { // , boost::match_extra)){
                // Print out matches
                // Skip 0 because it is the whole expression.  match_extra might cause this
                // These replacements will probably be concatenated together.
                //for(unsigned i = 1; i < what.size(); ++i){
                //    std::cout << "      $" << i << " = \"" << what[i] << "\"\n";
                //}
                return true; // Succeeded
            }
            return false;
        default:
            throw SpartaException("Unhandled name comparision value ") << name_comparison_
                                                                     << " in tree filter expression";
        };
    }

    /*!
     * \brief Dump this expression to a string (as a debug-level description)
     * \note This output cannot be reparsed as an expression
     */
    void Expression::dump(std::ostream& out) const {
        out << "{" << this << " "; // Address

        switch(op_){
        case OP_INVALID:
            out << "OP_INVALID";
            break;
        case OP_FALSE:
            out << "FALSE";
            break;
        case OP_TRUE:
            out << "TRUE";
            break;
        case OP_XOR:
            out << "XOR";
            break;
        case OP_OR:
            out << "OR";
            break;
        case OP_AND:
            out << "AND";
            break;
        case OP_NOT:
            out << "NOT";
            break;
        case OP_EVAL_VIS:
            out << "VIS ";
            switch(vis_comparison_){
            case VISCOMP_EQ:
                out << "==";
                break;
            case VISCOMP_GT:
                out << ">";
                break;
            case VISCOMP_LT:
                out << "<";
                break;
            case VISCOMP_GE:
                out << ">=";
                break;
            case VISCOMP_LE:
                out << "<=";
                break;
            case VISCOMP_NE:
                out << "!=";
                break;
            default:
                out << "<unhandled vis comparison: " << vis_comparison_ << ">";
            }
            out << " " << visibility_;
            break;
        case OP_EVAL_TYPE:
            out << "TYPE ";
            switch(type_comparison_){
            case TYPECOMP_EQ:
                out << "==";
                break;
            case TYPECOMP_NE:
                out << "!=";
                break;
            default:
                out << "<unhandled type comparison: " << type_comparison_ << ">";
            }
            out << " " << instrument_type_;
            break;
        case OP_EVAL_TAG:
            out << "TAG ";
            switch(tag_comparison_){
            case TAGCOMP_EQ:
                out << "==";
                break;
            case TAGCOMP_NE:
                out << "!=";
                break;
            case TAGCOMP_REM:
                out << " regex-match ";
                break;
            default:
                out << "<unhandled tag comparison: " << tag_comparison_ << ">";
            }
            out << " \"" << tag_ << "\"";
            break;
        case OP_EVAL_NAME:
            out << "NAME ";
            switch(name_comparison_){
            case NAMECOMP_EQ:
                out << "==";
                break;
            case NAMECOMP_NE:
                out << "!=";
                break;
            case NAMECOMP_REM:
                out << " regex-match ";
                break;
            default:
                out << "<unhandled name comparison: " << name_comparison_ << ">";
            }
            out << " \"" << name_ << "\"";
            break;
        default:
            out << "<unhandled operation: " << op_ << ">";
        }

        if(operands_.size() > 0){
            out << " : ";
            for(auto& x : operands_){
                out << x << " ";
            }
            out << "}";
        }
    }

} // namespace filter
} // namespace tree
} // namespace sparta
