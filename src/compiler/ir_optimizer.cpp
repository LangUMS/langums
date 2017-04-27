#include "ir_optimizer.h"

namespace Langums
{

    std::vector<std::unique_ptr<IIRInstruction>> IROptimizer::Process(std::vector<std::unique_ptr<IIRInstruction>> instructions)
    {
        m_Instructions = std::move(instructions);

        while (EliminateRedundantPushPopPairs()) {}

        return std::move(m_Instructions);
    }

    bool IROptimizer::EliminateRedundantPushPopPairs()
    {
        bool madeChanges = false;

        for (auto i = 0u; i < m_Instructions.size() - 1; i++)
        {
            auto& current = m_Instructions[i];
            auto& next = m_Instructions[i + 1];
            
            if (current->GetType() == IRInstructionType::Pop &&
                next->GetType() == IRInstructionType::Push)
            {
                auto pop = (IRPopInstruction*)current.get();
                auto push = (IRPushInstruction*)next.get();

                if (push->GetRegisterId() == pop->GetRegisterId())
                {
                    m_Instructions[i] = std::make_unique<IRNopInstruction>();
                    m_Instructions[i + 1] = std::make_unique<IRNopInstruction>();
                    madeChanges = true;
                }
            }
            else if (current->GetType() == IRInstructionType::Push &&
                next->GetType() == IRInstructionType::Pop)
            {
                auto push = (IRPushInstruction*)current.get();
                auto pop = (IRPopInstruction*)next.get();

                if (push->GetRegisterId() == pop->GetRegisterId())
                {
                    m_Instructions[i] = std::make_unique<IRNopInstruction>();
                    m_Instructions[i + 1] = std::make_unique<IRNopInstruction>();
                    madeChanges = true;
                }
            }
        }

        return madeChanges;
    }

}
