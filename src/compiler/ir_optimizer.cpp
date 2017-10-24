#include "ir_optimizer.h"

namespace LangUMS
{

    std::vector<std::unique_ptr<IIRInstruction>> IROptimizer::Process(std::vector<std::unique_ptr<IIRInstruction>> instructions)
    {
        m_Instructions = std::move(instructions);

        return std::move(m_Instructions);
    }

    bool IROptimizer::EliminateRedundantPushPopPairs()
    {
        auto jmpTargets = CalculateJmpTargets(m_Instructions);

        bool madeChanges = false;

        for (auto i = 0u; i < m_Instructions.size() - 1u; i++)
        {
            auto& current = m_Instructions[i];
            auto& next = m_Instructions[i + 1];
            
            if (current->GetType() == IRInstructionType::Push &&
                next->GetType() == IRInstructionType::Pop)
            {
                auto push = (IRPushInstruction*)current.get();
                auto pop = (IRPopInstruction*)next.get();

                if (push->GetRegisterId() == pop->GetRegisterId() ||
                    pop->GetRegisterId() == -1)
                {
                    if (jmpTargets[i] || jmpTargets[i + 1])
                    {
                        continue; // we shouldn't optimize out jmp targets
                    }

                    m_Instructions[i] = std::make_unique<IRNopInstruction>();
                    m_Instructions[i + 1] = std::make_unique<IRNopInstruction>();
                    madeChanges = true;
                }
            }
        }

        return madeChanges;
    }

    std::vector<bool> IROptimizer::CalculateJmpTargets(const std::vector<std::unique_ptr<IIRInstruction>>& instructions)
    {
        std::vector<bool> jmpTargets;
        jmpTargets.resize(instructions.size());

        for (auto i = 0u; i < instructions.size(); i++)
        {
            auto& instruction = instructions[i];
            if (instruction->GetType() == IRInstructionType::Jmp)
            {
                auto jmp = (IRJmpInstruction*)instruction.get();
                auto offset = jmp->GetOffset();

                if (!jmp->IsAbsolute())
                {
                    offset += i;
                }

                if (offset >= (int)jmpTargets.size())
                {
                    throw IRCompilerException("Out of bounds jump", 0);
                }

                jmpTargets[offset] = true;
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfEq)
            {
                auto jmp = (IRJmpIfEqInstruction*)instruction.get();
                auto offset = jmp->GetOffset();

                if (!jmp->IsAbsolute())
                {
                    offset += i;
                }

                if (offset >= (int)jmpTargets.size())
                {
                    throw IRCompilerException("Out of bounds jump", 0);
                }

                jmpTargets[offset] = true;
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfNotEq)
            {
                auto jmp = (IRJmpIfNotEqInstruction*)instruction.get();
                auto offset = jmp->GetOffset();

                if (!jmp->IsAbsolute())
                {
                    offset += i;
                }

                if (offset >= (int)jmpTargets.size())
                {
                    throw IRCompilerException("Out of bounds jump", 0);
                }

                jmpTargets[offset] = true;
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfSwSet)
            {
                auto jmp = (IRJmpIfSwSetInstruction*)instruction.get();
                auto offset = jmp->GetOffset();

                if (!jmp->IsAbsolute())
                {
                    offset += i;
                }

                if (offset >= (int)jmpTargets.size())
                {
                    throw IRCompilerException("Out of bounds jump", 0);
                }

                jmpTargets[offset] = true;
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfSwNotSet)
            {
                auto jmp = (IRJmpIfSwNotSetInstruction*)instruction.get();
                auto offset = jmp->GetOffset();

                if (!jmp->IsAbsolute())
                {
                    offset += i;
                }

                if (offset >= (int)jmpTargets.size())
                {
                    throw IRCompilerException("Out of bounds jump", 0);
                }

                jmpTargets[offset] = true;
            }
        }

        return jmpTargets;
    }

}
