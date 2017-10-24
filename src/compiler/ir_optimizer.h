#ifndef __LANGUMS_IR_OPTIMIZE_H
#define __LANGUMS_IR_OPTIMIZE_H

#include "ir.h"

namespace LangUMS
{

    class IROptimizer
    {
        public:
        std::vector<std::unique_ptr<IIRInstruction>> Process(std::vector<std::unique_ptr<IIRInstruction>> instructions);

        private:
        bool EliminateRedundantPushPopPairs();

        std::vector<std::unique_ptr<IIRInstruction>> m_Instructions;
        std::vector<bool> CalculateJmpTargets(const std::vector<std::unique_ptr<IIRInstruction>>& instructions);
    };

}

#endif
