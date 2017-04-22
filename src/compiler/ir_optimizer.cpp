#include "ir_optimizer.h"

namespace Langums
{

	std::vector<std::unique_ptr<IIRInstruction>> IROptimizer::Process(std::vector<std::unique_ptr<IIRInstruction>> instructions)
	{
		m_Instructions = std::move(instructions);

		//EliminateRedundantStores();

		return std::move(m_Instructions);
	}

	void IROptimizer::EliminateRedundantStores()
	{
		std::vector<bool> redundancies;
		redundancies.resize(m_Instructions.size(), false);

		for (auto i = 0u; i < m_Instructions.size() - 1; i++)
		{
			auto& current = m_Instructions[i];
			auto& next = m_Instructions[i + 1];

			if (next->GetType() == IRInstructionType::SetReg)
			{
				auto nextSetReg = (IRSetRegInstruction*)next.get();

				if (current->GetType() == IRInstructionType::SetReg)
				{
					auto currentSetReg = (IRSetRegInstruction*)current.get();

					if (nextSetReg->GetRegisterId() == currentSetReg->GetRegisterId())
					{
						redundancies[i] = true;
					}
				}
			}
		}

		std::vector<std::unique_ptr<IIRInstruction>> output;

		for (auto i = 0u; i < m_Instructions.size() - 1; i++)
		{
			if (!redundancies[i])
			{
				output.push_back(std::move(m_Instructions[i]));
			}
		}

		m_Instructions = std::move(output);
	}

}
