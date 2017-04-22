#ifndef __LANGUMS_IR_OPTIMIZE_H
#define __LANGUMS_IR_OPTIMIZE_H

#include "ir.h"

namespace Langums
{

	class IROptimizer
	{
		public:
		std::vector<std::unique_ptr<IIRInstruction>> Process(std::vector<std::unique_ptr<IIRInstruction>> instructions);

		private:
		void EliminateRedundantStores();

		std::vector<std::unique_ptr<IIRInstruction>> m_Instructions;
	};

}

#endif
