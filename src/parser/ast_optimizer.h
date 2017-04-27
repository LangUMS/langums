#ifndef __LANGUMS_ASTOPTIMIZER_H
#define __LANGUMS_ASTOPTIMIZER_H

#include "../ast/ast.h"

namespace Langums
{

    class ASTOptimizer
    {
        public:
        std::shared_ptr<IASTNode> Process(std::shared_ptr<IASTNode> ast);

        private:
        std::shared_ptr<IASTNode> CalculateConstantExpressions(const std::shared_ptr<IASTNode>& node);

        int CalculateConstantBinaryExpression(int left, int right, OperatorType op);

        std::shared_ptr<IASTNode> m_Root;
    };

}

#endif
