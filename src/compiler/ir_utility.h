#ifndef __LANGUMS_IRUTILITY_H
#define __LANGUMS_IRUTILITY_H

#include "ir.h"

namespace Langums
{

    struct StackFrame
    {
        std::string m_FunctionName;
        std::vector<std::pair<unsigned int, std::string>> m_Variables;
        IASTNode* m_ASTNode = nullptr;
    };

    inline std::string RegisterIdToString(unsigned int regId)
    {
        if (regId == Reg_InstructionCounter)
        {
            return "[IC]";
        }
        else if (regId == Reg_Temp0)
        {
            return "[TEMP 0]";
        }
        else if (regId == Reg_Temp1)
        {
            return "[TEMP 1]";
        }
        else if (regId >= Reg_StackTop)
        {
            return SafePrintf("[STACK %]", regId - Reg_StackTop);
        }
        else if (regId <= 200)
        {
            return SafePrintf("r%", regId);
        }
        else
        {
            return SafePrintf("-- INVALID REGISTER --");
        }
    }

    inline std::string SwitchToString(unsigned int switchId)
    {
        if (switchId == Switch_ArithmeticUnderflow)
        {
            return "[UNDERFLOW]";
        }

        return SafePrintf("[SWITCH %]", switchId);
    }

}

#endif
