// <Expression> -*- C++ -*-

#include "sparta/statistics/Expression.hpp"
#include "sparta/statistics/ExpressionParser.hpp"

#include <iostream>
#include <vector>

namespace sparta {
    namespace statistics {
        namespace expression {

Expression::Expression(const std::string& expression,
                       TreeNode* context)
{
    std::vector<const TreeNode*> already_used;
    const std::vector<stat_pair_t> report_si;
    parse_(expression, context, already_used, report_si);
}

Expression::Expression(const std::string& expression,
                       TreeNode* context,
                       std::vector<const TreeNode*>& already_used)
{
    const std::vector<stat_pair_t> report_si;
    parse_(expression, context, already_used, report_si);

}
Expression::Expression(const std::string& expression,
                       TreeNode* context,
                       const std::vector<stat_pair_t>&report_si)
{
    std::vector<const TreeNode*> already_used;
    parse_(expression, context, already_used, report_si);
}

Expression::Expression(const TreeNode* n, std::vector<const TreeNode*>& used) :
    Expression(new StatVariable(n, used))
{
    // Delegated Constructor
}

Expression::~Expression() {
    //std::cout << "Destruction  " << this << std::endl;
}

void Expression::parse_(const std::string& expression,
                        TreeNode* context,
                        std::vector<const TreeNode*>& already_used,
                        const std::vector<stat_pair_t> &report_si)
{
    sparta_assert(context, "cannot parse an expression \"" << expression
                               << "\" without a null context");
    ExpressionParser parser(context, already_used, report_si);
    try{
        Expression ex = parser.parse(expression);
        content_ = std::move(ex.content_);
    }catch(SpartaException &ex){
        throw SpartaException("Failed to parse expression \"") << expression << "\" in context of "
            "node \"" << context->getLocation() << "\" for the following reason: " << ex.what();
    }
}

        } // namespace expression
    } // statistics
} // namespace sparta
