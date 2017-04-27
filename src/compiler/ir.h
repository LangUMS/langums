#ifndef __LANGUMS_IR_H
#define __LANGUMS_IR_H

#include <memory>
#include <unordered_map>

#include "../ast/ast.h"
#include "../libchk/chk.h"
#include "../stringutil.h"

#define MAX_REGISTER_INDEX 224
#define MAX_EVENT_CONDITIONS 64

namespace Langums
{

    using namespace CHK;

    enum ReservedRegisters
    {
        Reg_InstructionCounter = 0,
        Reg_CopyStorage,
        Reg_FunctionReturn,
        Reg_FunctionArg0,
        Reg_FunctionArg1,
        Reg_FunctionArg2,
        Reg_FunctionArg3,
        Reg_FunctionArg4,
        Reg_FunctionArg5,
        Reg_FunctionArg6,
        Reg_FunctionArg7,
        Reg_Temp0,
        Reg_Temp1,
        Reg_Temp2,
        Reg_Reserved0,
        Reg_Reserved1,
        Reg_Reserved2,
        Reg_Reserved3,
        Reg_Reserved4,
        Reg_Reserved5,
        Reg_Reserved6,
        Reg_Reserved7,
        Reg_Reserved8,
        Reg_Reserved9,
        Reg_Reserved10,
        Reg_Reserved11,
        Reg_ReservedEnd,

        Reg_StackTop = 0x574C
    };

    enum ReservedSwitches
    {
        Switch_InstructionCounterMutex,
        Switch_EventsMutex,
        Switch_ArithmeticUnderflow,
        Switch_ReservedEnd
    };

    enum class IRInstructionType
    {
        None = 0,
        // core
        Push, // pushes value on top of the stack and decrements the stack pointer
        Pop, // pops a value from the stack and increments the stack pointer
        Call, // calls a function, pushes return address on the stack
        Ret, // pops return address from the stack and jumps to it
        SetReg, // sets a register to a constant value
        IncReg, // increments a register by a constant value
        DecReg, // decrements a register by a constant value
        CopyReg, // copies a register's value to another register
        Add, // pops two values off the stack and adds them together, pushes the result on the stack
        Sub, // pops two values off the stack, subtracts the second from the first, pushes the result on the stack
        Jmp, // jumps to an instruction using relative or absolute offset
        JmpIfEqZero, // jumps to an instruction if a register is equal to zero,
        JmpIfNotEqZero, // jumps to an instruction if a register is not equal to zero
        JmpIfSwNotSet, // jumps to an instruction if a switch is not set
        JmpIfSwSet, // jumps to an instruction if a switch is set
        SetSw, // sets a switch
        // actions
        Wait, // waits for an amount of time
        DisplayMsg, // displays a text message
        Spawn, // spawns a unit
        Kill, // kills a unit
        Move, // moves a unit
        Order, // orders a unit to move, attack or patrol
        Modify, // modifies a unit's properties such as hp, energy, shields and hangar count
        MoveLoc, // moves a location
        EndGame, // ends the game in a victory or defeat for a given player
        CenterView, // centers the camera on a location for a given player
        Ping, // Minimap ping at a location
        SetResource, // sets the resource count for a given resource for a given player
        IncResource, // increments the resource count for a given resource for a given player
        DecResource, // decrements the resource count for a given resource for a given player
        SetCountdown, // sets the countdown timer, 
        // conditions
        Event,
        BringCond, // Bring trigger condition
        AccumCond, // Accumulate trigger condition
        TimeCond, // Elapsed time trigger condition
        CmdCond, // Player commands a quantity of units condition
        KillCond, // Player has killed a quantity of units condition
        DeathCond, // Player has lots a quantity of units condition
        CountdownCond, // Countdown timer condition
    };

    enum class ConditionComparison
    {
        AtLeast = 0,
        AtMost = 1,
        Exactly = 10
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
        else if (regId == Reg_Temp2)
        {
            return "[TEMP 2]";
        }
        else if (regId == Reg_FunctionReturn)
        {
            return "[RETURN]";
        }
        else if (regId >= Reg_StackTop)
        {
            return SafePrintf("s%", regId - Reg_StackTop);
        }
        else if(regId <= 200)
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
        else if (switchId == Switch_EventsMutex)
        {
            return "[EVNT MUTEX]";
        }
        
        return SafePrintf("[SWITCH %]", switchId);
    }

    class IIRInstruction
    {
        public:
        IIRInstruction(IRInstructionType type) : m_Type(type) {}
        virtual ~IIRInstruction() {}

        IRInstructionType GetType() const
        {
            return m_Type;
        }

        virtual std::string DebugDump() const = 0;

        private:
        IRInstructionType m_Type = IRInstructionType::None;
    };

    class IRPushInstruction : public IIRInstruction
    {
        public:
        IRPushInstruction(unsigned int regId, bool isLiteral = false) :
            m_RegId(regId), m_IsValueLiteral(isLiteral), IIRInstruction(IRInstructionType::Push) {}

        unsigned int GetRegisterId() const
        {
            return m_RegId;
        }

        bool IsValueLiteral() const
        {
            return m_IsValueLiteral;
        }

        std::string DebugDump() const
        {
            if (m_IsValueLiteral)
            {
                return SafePrintf("PUSH %", m_RegId);
            }
            else
            {
                return SafePrintf("PUSH %", RegisterIdToString(m_RegId));
            }
        }

        private:
        unsigned int m_RegId = 0;
        bool m_IsValueLiteral = false;
    };

    class IRPopInstruction : public IIRInstruction
    {
        public:
        IRPopInstruction(int regId = -1) : // if regId == -1 will only increment the stack pointer without copying the value into a register
            m_RegId(regId), IIRInstruction(IRInstructionType::Pop) {}

        unsigned int GetRegisterId() const
        {
            return m_RegId;
        }

        std::string DebugDump() const
        {
            if (m_RegId != -1)
            {
                return SafePrintf("POP %", RegisterIdToString(m_RegId));
            }
            else
            {
                return "POP";
            }
        }

        private:
        int m_RegId = 0;
    };

    class IRSetRegInstruction : public IIRInstruction
    {
        public:
        IRSetRegInstruction(unsigned int regId, int value) :
            m_RegId(regId), m_Value(value), IIRInstruction(IRInstructionType::SetReg) {}

        unsigned int GetRegisterId() const
        {
            return m_RegId;
        }

        int GetValue() const
        {
            return m_Value;
        }

        std::string DebugDump() const
        {
            return SafePrintf("SET % %", RegisterIdToString(m_RegId), m_Value);
        }

        private:
        unsigned int m_RegId = 0;
        int m_Value = 0;
    };

    class IRIncRegInstruction : public IIRInstruction
    {
        public:
        IRIncRegInstruction(unsigned int regId, int amount) :
            m_RegId(regId), m_Amount(amount), IIRInstruction(IRInstructionType::IncReg) {}

        unsigned int GetRegisterId() const
        {
            return m_RegId;
        }

        int GetAmount() const
        {
            return m_Amount;
        }

        std::string DebugDump() const
        {
            if (m_Amount > 1)
            {
                return SafePrintf("INC % %", RegisterIdToString(m_RegId), m_Amount);
            }

            return SafePrintf("INC %", RegisterIdToString(m_RegId));
        }

        private:
        unsigned int m_RegId = 0;
        int m_Amount = 0;
    };

    class IRDecRegInstruction : public IIRInstruction
    {
        public:
        IRDecRegInstruction(unsigned int regId, int amount) :
            m_RegId(regId), m_Amount(amount), IIRInstruction(IRInstructionType::DecReg) {}

        unsigned int GetRegisterId() const
        {
            return m_RegId;
        }

        int GetAmount() const
        {
            return m_Amount;
        }

        std::string DebugDump() const
        {
            if (m_Amount > 1)
            {
                return SafePrintf("DEC % %", RegisterIdToString(m_RegId), m_Amount);
            }

            return SafePrintf("DEC %", RegisterIdToString(m_RegId));
        }

        private:
        unsigned int m_RegId = 0;
        int m_Amount = 0;
    };

    class IRCopyRegInstruction : public IIRInstruction
    {
        public:
        IRCopyRegInstruction(unsigned int dstRegId, unsigned int srcRegId) :
            m_DstRegId(dstRegId), m_SrcRegId(srcRegId), IIRInstruction(IRInstructionType::CopyReg) {}

        unsigned int GetDestinationRegisterId() const
        {
            return m_DstRegId;
        }

        unsigned int GetSourceRegisterId() const
        {
            return m_SrcRegId;
        }

        std::string DebugDump() const
        {
            return SafePrintf("CPY % %", RegisterIdToString(m_DstRegId), RegisterIdToString(m_SrcRegId));
        }

        private:
        unsigned int m_DstRegId = 0;
        unsigned int m_SrcRegId = 0;
    };

    class IRAddInstruction : public IIRInstruction
    {
        public:
        IRAddInstruction() : IIRInstruction(IRInstructionType::Add) {}

        std::string DebugDump() const
        {
            return "ADD";
        }
    };

    class IRSubInstruction : public IIRInstruction
    {
        public:
        IRSubInstruction() : IIRInstruction(IRInstructionType::Sub) {}

        std::string DebugDump() const
        {
            return "SUB";
        }
    };

    class IRDisplayMsgInstruction : public IIRInstruction
    {
        public:
        IRDisplayMsgInstruction(const std::string& message, unsigned char playerMask) :
            m_Message(message), m_PlayerMask(playerMask), IIRInstruction(IRInstructionType::DisplayMsg) {}

        const std::string& GetMessage() const
        {
            return m_Message;
        }

        char GetPlayerMask() const
        {
            return m_PlayerMask;
        }

        std::string DebugDump() const
        {
            return SafePrintf("MSG \"%\" %", m_Message, (int)m_PlayerMask);
        }

        private:
        std::string m_Message;
        char m_PlayerMask;
    };

    class IRJmpInstruction : public IIRInstruction
    {
        public:
        IRJmpInstruction(int offset, bool absolute = false) :
            m_Offset(offset), m_IsAbsolute(absolute), IIRInstruction(IRInstructionType::Jmp) {}

        int GetOffset() const
        {
            return m_Offset;
        }

        void SetOffset(int offset)
        {
            m_Offset = offset;
        }

        bool IsAbsolute() const
        {
            return m_IsAbsolute;
        }

        std::string DebugDump() const
        {
            if (m_IsAbsolute)
            {
                return SafePrintf("JMP %", m_Offset);
            }
            else
            {
                if (m_Offset >= 0)
                {
                    return SafePrintf("JMP +%", m_Offset);
                }
                else
                {
                    return SafePrintf("JMP %", m_Offset);

                }
            }
        }

        private:
        int m_Offset = 0;
        bool m_IsAbsolute = false;
    };

    class IRJmpIfEqZeroInstruction : public IIRInstruction
    {
        public:
        IRJmpIfEqZeroInstruction(unsigned int regId, int offset, bool absolute = false) :
            m_RegisterId(regId), m_Offset(offset), m_IsAbsolute(absolute), IIRInstruction(IRInstructionType::JmpIfEqZero) {}

        int GetOffset() const
        {
            return m_Offset;
        }

        unsigned int GetRegisterId() const
        {
            return m_RegisterId;
        }

        bool IsAbsolute() const
        {
            return m_IsAbsolute;
        }

        std::string DebugDump() const
        {
            if (m_IsAbsolute)
            {
                return SafePrintf("JEZ % %", RegisterIdToString(m_RegisterId), m_Offset);
            }
            else
            {
                if (m_Offset >= 0)
                {
                    return SafePrintf("JEZ % +%", RegisterIdToString(m_RegisterId), m_Offset);
                }
                else
                {
                    return SafePrintf("JEZ % %", RegisterIdToString(m_RegisterId), m_Offset);

                }
            }
        }

        private:
        unsigned int m_RegisterId = 0;
        int m_Offset = 0;
        bool m_IsAbsolute = false;
    };

    class IRJmpIfNotEqZeroInstruction : public IIRInstruction
    {
        public:
        IRJmpIfNotEqZeroInstruction(unsigned int regId, int offset, bool absolute = false) :
            m_RegisterId(regId), m_Offset(offset), m_IsAbsolute(absolute), IIRInstruction(IRInstructionType::JmpIfNotEqZero) {}

        int GetOffset() const
        {
            return m_Offset;
        }

        unsigned int GetRegisterId() const
        {
            return m_RegisterId;
        }

        bool IsAbsolute() const
        {
            return m_IsAbsolute;
        }

        std::string DebugDump() const
        {
            if (m_IsAbsolute)
            {
                return SafePrintf("JNZ % %", RegisterIdToString(m_RegisterId), m_Offset);
            }
            else
            {
                if (m_Offset >= 0)
                {
                    return SafePrintf("JNZ % +%", RegisterIdToString(m_RegisterId), m_Offset);
                }
                else
                {
                    return SafePrintf("JNZ % %", RegisterIdToString(m_RegisterId), m_Offset);

                }
            }
        }

        private:
        unsigned int m_RegisterId = 0;
        int m_Offset = 0;
        bool m_IsAbsolute = false;
    };

    class IRJmpIfSwNotSetInstruction : public IIRInstruction
    {
        public:
        IRJmpIfSwNotSetInstruction(unsigned int switchId, int offset, bool absolute = false) :
            m_SwitchId(switchId), m_Offset(offset), m_IsAbsolute(absolute), IIRInstruction(IRInstructionType::JmpIfSwNotSet) {}

        int GetOffset() const
        {
            return m_Offset;
        }

        unsigned int GetSwitchId() const
        {
            return m_SwitchId;
        }

        bool IsAbsolute() const
        {
            return m_IsAbsolute;
        }

        std::string DebugDump() const
        {
            if (m_IsAbsolute)
            {
                return SafePrintf("JSNS % %", SwitchToString(m_SwitchId), m_Offset);
            }
            else
            {
                if (m_Offset >= 0)
                {
                    return SafePrintf("JSNS % +%", SwitchToString(m_SwitchId), m_Offset);
                }
                else
                {
                    return SafePrintf("JSNS % %", SwitchToString(m_SwitchId), m_Offset);

                }
            }
        }

        private:
        unsigned int m_SwitchId = 0;
        int m_Offset = 0;
        bool m_IsAbsolute = false;
    };

    class IRJmpIfSwSetInstruction : public IIRInstruction
    {
        public:
        IRJmpIfSwSetInstruction(unsigned int switchId, int offset, bool absolute = false) :
            m_SwitchId(switchId), m_Offset(offset), m_IsAbsolute(absolute), IIRInstruction(IRInstructionType::JmpIfSwSet) {}

        int GetOffset() const
        {
            return m_Offset;
        }

        unsigned int GetSwitchId() const
        {
            return m_SwitchId;
        }

        bool IsAbsolute() const
        {
            return m_IsAbsolute;
        }

        std::string DebugDump() const
        {
            if (m_IsAbsolute)
            {
                return SafePrintf("JSS % %", SwitchToString(m_SwitchId), m_Offset);
            }
            else
            {
                if (m_Offset >= 0)
                {
                    return SafePrintf("JSS % +%", SwitchToString(m_SwitchId), m_Offset);
                }
                else
                {
                    return SafePrintf("JSS % %", SwitchToString(m_SwitchId), m_Offset);

                }
            }
        }

        private:
        unsigned int m_SwitchId = 0;
        int m_Offset = 0;
        bool m_IsAbsolute = false;
    };

    class IRSetSwInstruction : public IIRInstruction
    {
        public:
        IRSetSwInstruction(unsigned int switchId, bool state) :
            m_SwitchId(switchId), m_State(state), IIRInstruction(IRInstructionType::SetSw) {}

        unsigned int GetSwitchId() const
        {
            return m_SwitchId;
        }

        bool GetState() const
        {
            return m_State;
        }

        std::string DebugDump() const
        {
            return SafePrintf("SETSW % %", SwitchToString(m_SwitchId), m_State);
        }

        private:
        unsigned int m_SwitchId = 0;
        bool m_State;
    };

    class IRWaitInstruction : public IIRInstruction
    {
        public:
        IRWaitInstruction(unsigned int milliseconds) :
            m_Milliseconds(milliseconds), IIRInstruction(IRInstructionType::Wait) {}

        unsigned int GetMilliseconds() const
        {
            return m_Milliseconds;
        }

        std::string DebugDump() const
        {
            return SafePrintf("WAIT %", m_Milliseconds);
        }

        private:
        unsigned int m_Milliseconds = 0;
    };

    class IRCallInstruction : public IIRInstruction
    {
        public:
        IRCallInstruction(const std::string& fnName, unsigned int index, unsigned int returnRegId) :
            m_Index(index), m_ReturnRegId(returnRegId), m_FunctionName(fnName), IIRInstruction(IRInstructionType::Call) {}

        unsigned int GetIndex() const
        {
            return m_Index;
        }

        void SetIndex(unsigned int index)
        {
            m_Index = index;
        }

        unsigned int GetReturnRegisterId() const
        {
            return m_ReturnRegId;
        }

        const std::string& GetFunctionName()
        {
            return m_FunctionName;
        }

        std::string DebugDump() const
        {
            return SafePrintf("CALL % %", m_Index, RegisterIdToString(m_ReturnRegId));
        }

        private:
        std::string m_FunctionName;
        unsigned int m_Index = 0;
        unsigned int m_ReturnRegId = 0;
    };

    class IRRetInstruction : public IIRInstruction
    {
        public:
        IRRetInstruction(unsigned int regId) :
            m_RegisterId(regId), IIRInstruction(IRInstructionType::Ret) {}

        std::string DebugDump() const
        {
            return SafePrintf("RET %", RegisterIdToString(m_RegisterId));
        }

        unsigned int GetRegisterId() const
        {
            return m_RegisterId;
        }

        private:
        unsigned int m_RegisterId = 0;
    };

    class IRSpawnInstruction : public IIRInstruction
    {
        public:
        IRSpawnInstruction(uint8_t playerId, uint8_t unitId, uint32_t regId, bool isValueLiteral, const std::string& locationName) :
            m_PlayerId(playerId), m_UnitId(unitId), m_RegId(regId), m_IsLiteralValue(isValueLiteral), m_LocationName(locationName),
            IIRInstruction(IRInstructionType::Spawn) {}

        std::string DebugDump() const
        {
            if (m_IsLiteralValue)
            {
                return SafePrintf("SPAWN % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], m_RegId, m_LocationName);
            }

            return SafePrintf("SPAWN % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], RegisterIdToString(m_RegId), m_LocationName);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        uint32_t GetRegisterId() const
        {
            return m_RegId;
        }

        bool IsValueLiteral() const
        {
            return m_IsLiteralValue;
        }

        const std::string& GetLocationName() const
        {
            return m_LocationName;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        uint32_t m_RegId;
        bool m_IsLiteralValue = false;
        std::string m_LocationName;
    };

    class IRKillInstruction : public IIRInstruction
    {
        public:
        IRKillInstruction(uint8_t playerId, uint8_t unitId, uint32_t regId, bool isValueLiteral, const std::string& locationName) :
            m_PlayerId(playerId), m_UnitId(unitId), m_RegId(regId), m_IsLiteralValue(isValueLiteral), m_LocationName(locationName),
            IIRInstruction(IRInstructionType::Kill) {}

        std::string DebugDump() const
        {
            if (m_IsLiteralValue)
            {
                return SafePrintf("KILL % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], m_RegId, m_LocationName);
            }

            return SafePrintf("KILL % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], RegisterIdToString(m_RegId), m_LocationName);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        uint32_t GetRegisterId() const
        {
            return m_RegId;
        }

        bool IsValueLiteral() const
        {
            return m_IsLiteralValue;
        }

        const std::string& GetLocationName() const
        {
            return m_LocationName;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        uint32_t m_RegId;
        bool m_IsLiteralValue;
        std::string m_LocationName;
    };

    class IRMoveInstruction : public IIRInstruction
    {
        public:
        IRMoveInstruction(uint8_t playerId, uint8_t unitId, uint32_t regId, bool isValueLiteral, const std::string& srcLocation, const std::string& dstLocation) :
            m_PlayerId(playerId), m_UnitId(unitId), m_RegId(regId), m_IsLiteralValue(isValueLiteral),
            m_SrcLocationName(srcLocation), m_DstLocationName(dstLocation), IIRInstruction(IRInstructionType::Move) {}

        std::string DebugDump() const
        {
            if (m_IsLiteralValue)
            {
                return SafePrintf("MOVE % % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], m_RegId, m_SrcLocationName, m_DstLocationName);
            }

            return SafePrintf("MOVE % % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], RegisterIdToString(m_RegId), m_SrcLocationName, m_DstLocationName);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        uint32_t GetRegisterId() const
        {
            return m_RegId;
        }

        bool IsValueLiteral() const
        {
            return m_IsLiteralValue;
        }

        const std::string& GetSrcLocationName() const
        {
            return m_SrcLocationName;
        }

        const std::string& GetDstLocationName() const
        {
            return m_DstLocationName;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        uint32_t m_RegId;
        bool m_IsLiteralValue;
        std::string m_SrcLocationName;
        std::string m_DstLocationName;
    };

    class IROrderInstruction : public IIRInstruction
    {
        public:
        IROrderInstruction(uint8_t playerId, uint8_t unitId, TriggerActionState order, const std::string& srcLocation, const std::string& dstLocation) :
            m_PlayerId(playerId), m_UnitId(unitId), m_Order(order),
            m_SrcLocationName(srcLocation), m_DstLocationName(dstLocation), IIRInstruction(IRInstructionType::Order) {}

        std::string DebugDump() const
        {
            std::string order;
            switch (m_Order)
            {
            case TriggerActionState::Move:
                order = "Move";
                break;
            case TriggerActionState::Attack:
                order = "Attack";
                break;
            case TriggerActionState::Patrol:
                order = "Patrol";
                break;
            }

            return SafePrintf("ORDER % % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], order, m_SrcLocationName, m_DstLocationName);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }
        
        TriggerActionState GetOrder() const
        {
            return m_Order;
        }

        const std::string& GetSrcLocationName() const
        {
            return m_SrcLocationName;
        }

        const std::string& GetDstLocationName() const
        {
            return m_DstLocationName;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        TriggerActionState m_Order;
        std::string m_SrcLocationName;
        std::string m_DstLocationName;
    };

    enum class ModifyType
    {
        HitPoints,
        Energy,
        ShieldPoints,
        HangarCount
    };

    class IRModifyInstruction : public IIRInstruction
    {
        public:
        IRModifyInstruction(uint8_t playerId, uint8_t unitId, uint32_t regId, bool isValueLiteral, uint32_t amount, ModifyType modifyType, const std::string& locationName) :
            m_PlayerId(playerId), m_UnitId(unitId), m_RegId(regId), m_IsLiteralValue(isValueLiteral), m_Amount(amount), m_ModifyType(modifyType), m_LocationName(locationName),
            IIRInstruction(IRInstructionType::Modify) {}

        std::string DebugDump() const
        {
            std::string type;
            switch (m_ModifyType)
            {
            case ModifyType::HitPoints:
                type = "HitPoints";
                break;
            case ModifyType::Energy:
                type = "Energy";
                break;
            case ModifyType::ShieldPoints:
                type = "ShieldPoints";
                break;
            case ModifyType::HangarCount:
                type = "HangarCount";
                break;
            }

            if (m_IsLiteralValue)
            {
                return SafePrintf("MODIFY % % % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], m_RegId, type, m_Amount, m_LocationName);
            }

            return SafePrintf("MODIFY % % % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], RegisterIdToString(m_RegId), type, m_Amount, m_LocationName);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        uint32_t GetRegisterId() const
        {
            return m_RegId;
        }

        bool IsValueLiteral() const
        {
            return m_IsLiteralValue;
        }

        const std::string& GetLocationName() const
        {
            return m_LocationName;
        }

        uint32_t GetAmount() const
        {
            return m_Amount;
        }

        ModifyType GetModifyType() const
        {
            return m_ModifyType;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        uint32_t m_RegId;
        uint32_t m_Amount;
        bool m_IsLiteralValue = false;
        std::string m_LocationName;
        ModifyType m_ModifyType;
    };

    class IRMoveLocInstruction : public IIRInstruction
    {
        public:
        IRMoveLocInstruction(uint8_t playerId, uint8_t unitId, const std::string& srcLocation, const std::string& dstLocation) :
            m_PlayerId(playerId), m_UnitId(unitId), m_SrcLocationName(srcLocation), m_DstLocationName(dstLocation), IIRInstruction(IRInstructionType::MoveLoc) {}

        std::string DebugDump() const
        {
            return SafePrintf("MOVLOC % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], m_SrcLocationName, m_DstLocationName);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        const std::string& GetSrcLocationName() const
        {
            return m_SrcLocationName;
        }

        const std::string& GetDstLocationName() const
        {
            return m_DstLocationName;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        std::string m_SrcLocationName;
        std::string m_DstLocationName;
    };

    enum class EndGameType
    {
        Victory = 0,
        Defeat,
        Draw
    };

    class IREndGameInstruction : public IIRInstruction
    {
        public:
        IREndGameInstruction(uint8_t playerMask, EndGameType type) :
            m_PlayerMask(playerMask), m_EndGameType(type), IIRInstruction(IRInstructionType::EndGame) {}

        std::string DebugDump() const
        {
            std::string condition;
            switch (m_EndGameType)
            {
            case EndGameType::Victory:
                condition = "[VICTORY]";
                break;
            case EndGameType::Defeat:
                condition = "[DEFEAT]";
                break;
            case EndGameType::Draw:
                condition = "[DRAW]";
                break;
            }

            return SafePrintf("END % %", PlayersByName[m_PlayerMask], condition);
        }

        uint8_t GetPlayerMask() const
        {
            return m_PlayerMask;
        }

        EndGameType GetEndGameType() const
        {
            return m_EndGameType;
        }

        private:
        uint8_t m_PlayerMask;
        EndGameType m_EndGameType;
    };

    class IRCenterViewInstruction : public IIRInstruction
    {
        public:
        IRCenterViewInstruction(uint8_t playerId, const std::string& locationName) :
            m_PlayerId(playerId), m_LocationName(locationName),
            IIRInstruction(IRInstructionType::CenterView) {}

        std::string DebugDump() const
        {
            return SafePrintf("CNTRVIEW % %", PlayersByName[m_PlayerId], m_LocationName);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        const std::string& GetLocationName() const
        {
            return m_LocationName;
        }

        private:
        uint8_t m_PlayerId;
        std::string m_LocationName;
    };

    class IRPingInstruction : public IIRInstruction
    {
        public:
        IRPingInstruction(uint8_t playerId, const std::string& locationName) :
            m_PlayerId(playerId), m_LocationName(locationName),
            IIRInstruction(IRInstructionType::Ping) {}

        std::string DebugDump() const
        {
            return SafePrintf("PING % %", PlayersByName[m_PlayerId], m_LocationName);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        const std::string& GetLocationName() const
        {
            return m_LocationName;
        }

        private:
        uint8_t m_PlayerId;
        std::string m_LocationName;
    };

    class IRSetResourceInstruction : public IIRInstruction
    {
        public:
        IRSetResourceInstruction(uint8_t playerId, ResourceType resourceType, uint32_t regId, bool isLiteralValue) :
            m_PlayerId(playerId), m_ResourceType(resourceType), m_RegId(regId), m_IsValueLiteral(isLiteralValue),
            IIRInstruction(IRInstructionType::SetResource) {}

        std::string DebugDump() const
        {
            std::string qty;
            if (m_IsValueLiteral)
            {
                qty = SafePrintf("%", m_RegId);
            }
            else
            {
                qty = RegisterIdToString(m_RegId);
            }

            return SafePrintf("SETRSRC % % %", PlayersByName[m_PlayerId], m_ResourceType == ResourceType::Ore ? "Minerals" : "Gas", qty);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ResourceType GetResourceType() const
        {
            return m_ResourceType;
        }

        uint32_t GetRegisterId() const
        {
            return m_RegId;
        }
        
        bool IsValueLiteral() const
        {
            return m_IsValueLiteral;
        }

        private:
        uint8_t m_PlayerId;
        ResourceType m_ResourceType;
        uint32_t m_RegId;
        bool m_IsValueLiteral;
    };

    class IRIncResourceInstruction : public IIRInstruction
    {
        public:
        IRIncResourceInstruction(uint8_t playerId, ResourceType resourceType, uint32_t regId, bool isLiteralValue) :
            m_PlayerId(playerId), m_ResourceType(resourceType), m_RegId(regId), m_IsValueLiteral(isLiteralValue),
            IIRInstruction(IRInstructionType::IncResource) {}

        std::string DebugDump() const
        {
            std::string qty;
            if (m_IsValueLiteral)
            {
                qty = SafePrintf("%", m_RegId);
            }
            else
            {
                qty = RegisterIdToString(m_RegId);
            }

            return SafePrintf("INCRSRC % % %", PlayersByName[m_PlayerId], m_ResourceType == ResourceType::Ore ? "Minerals" : "Gas", qty);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ResourceType GetResourceType() const
        {
            return m_ResourceType;
        }

        uint32_t GetRegisterId() const
        {
            return m_RegId;
        }

        bool IsValueLiteral() const
        {
            return m_IsValueLiteral;
        }

        private:
        uint8_t m_PlayerId;
        ResourceType m_ResourceType;
        uint32_t m_RegId;
        bool m_IsValueLiteral;
    };

    class IRDecResourceInstruction : public IIRInstruction
    {
        public:
        IRDecResourceInstruction(uint8_t playerId, ResourceType resourceType, uint32_t regId, bool isLiteralValue) :
            m_PlayerId(playerId), m_ResourceType(resourceType), m_RegId(regId), m_IsValueLiteral(isLiteralValue),
            IIRInstruction(IRInstructionType::DecResource) {}

        std::string DebugDump() const
        {
            std::string qty;
            if (m_IsValueLiteral)
            {
                qty = SafePrintf("%", m_RegId);
            }
            else
            {
                qty = RegisterIdToString(m_RegId);
            }

            return SafePrintf("DECRSRC % % %", PlayersByName[m_PlayerId], m_ResourceType == ResourceType::Ore ? "Minerals" : "Gas", qty);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ResourceType GetResourceType() const
        {
            return m_ResourceType;
        }

        uint32_t GetRegisterId() const
        {
            return m_RegId;
        }

        bool IsValueLiteral() const
        {
            return m_IsValueLiteral;
        }

        private:
        uint8_t m_PlayerId;
        ResourceType m_ResourceType;
        uint32_t m_RegId;
        bool m_IsValueLiteral;
    };

    class IRSetCountdownInstruction : public IIRInstruction
    {
        public:
        IRSetCountdownInstruction(uint32_t regId, bool isLiteralValue) :
            m_RegisterId(regId), m_IsValueLiteral(isLiteralValue), IIRInstruction(IRInstructionType::SetCountdown) {}

        std::string DebugDump() const
        {
            if (m_IsValueLiteral)
            {
                return SafePrintf("SETCNDWN %", m_RegisterId);
            }
            else
            {
                return SafePrintf("SETCNDWN %", RegisterIdToString(m_RegisterId));
            }
        }

        uint32_t GetRegisterId() const
        {
            return m_RegisterId;
        }

        bool IsValueLiteral() const
        {
            return m_IsValueLiteral;
        }

        private:
        uint32_t m_RegisterId;
        bool m_IsValueLiteral;
    };

    class IREventInstruction : public IIRInstruction
    {
        public:
        IREventInstruction(unsigned int switchId, unsigned int conditionsCount) :
            m_SwitchId(switchId), m_ConditionsCount(conditionsCount), IIRInstruction(IRInstructionType::Event) {}

        std::string DebugDump() const
        {
            return SafePrintf("EVNT % %", SwitchToString(m_SwitchId), m_ConditionsCount);
        }

        unsigned int GetConditionsCount() const
        {
            return m_ConditionsCount;
        }

        unsigned int GetSwitchId() const
        {
            return m_SwitchId;
        }

        private:
        unsigned int m_ConditionsCount;
        unsigned int m_SwitchId;
    };

    class IRBringCondInstruction : public IIRInstruction
    {
        public:
        IRBringCondInstruction(uint8_t playerId, uint8_t unitId, const std::string& locationName, ConditionComparison comparison, uint32_t quantity) :
            m_PlayerId(playerId), m_UnitId(unitId), m_LocationName(locationName), m_Comparison(comparison), m_Quantity(quantity), IIRInstruction(IRInstructionType::BringCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("BRING % % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], m_LocationName, (int)m_Comparison, (int)m_Quantity);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        const std::string& GetLocationName() const
        {
            return m_LocationName;
        }

        ConditionComparison GetComparison() const
        {
            return m_Comparison;
        }

        uint32_t GetQuantity() const
        {
            return m_Quantity;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        std::string m_LocationName;
        ConditionComparison m_Comparison;
        uint32_t m_Quantity;
    };

    class IRAccumCondInstruction : public IIRInstruction
    {
        public:
        IRAccumCondInstruction(uint8_t playerId, ResourceType resourceType, ConditionComparison comparison, uint32_t quantity) :
            m_PlayerId(playerId), m_ResourceType(resourceType), m_Comparison(comparison), m_Quantity(quantity), IIRInstruction(IRInstructionType::AccumCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("ACCUM % % % %", PlayersByName[m_PlayerId], m_ResourceType == ResourceType::Ore ? "Minerals" : "Gas", (int)m_Comparison, (int)m_Quantity);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ResourceType GetResourceType() const
        {
            return m_ResourceType;
        }

        ConditionComparison GetComparison() const
        {
            return m_Comparison;
        }

        uint32_t GetQuantity() const
        {
            return m_Quantity;
        }

        private:
        uint8_t m_PlayerId;
        ResourceType m_ResourceType;
        ConditionComparison m_Comparison;
        uint32_t m_Quantity;
    };

    class IRTimeCondInstruction : public IIRInstruction
    {
        public:
        IRTimeCondInstruction(ConditionComparison comparison, uint32_t quantity) :
            m_Comparison(comparison), m_Quantity(quantity), IIRInstruction(IRInstructionType::TimeCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("TIME % %", (int)m_Comparison, (int)m_Quantity);
        }

        ConditionComparison GetComparison() const
        {
            return m_Comparison;
        }

        uint32_t GetQuantity() const
        {
            return m_Quantity;
        }

        private:
        ConditionComparison m_Comparison;
        uint32_t m_Quantity;
    };

    class IRCmdCondInstruction : public IIRInstruction
    {
        public:
        IRCmdCondInstruction(uint8_t playerId, uint8_t unitId, ConditionComparison comparison, uint32_t quantity) :
            m_PlayerId(playerId), m_UnitId(unitId), m_Comparison(comparison), m_Quantity(quantity), IIRInstruction(IRInstructionType::CmdCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("CMDS % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], (int)m_Comparison, (int)m_Quantity);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        ConditionComparison GetComparison() const
        {
            return m_Comparison;
        }

        uint32_t GetQuantity() const
        {
            return m_Quantity;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        ConditionComparison m_Comparison;
        uint32_t m_Quantity;
    };

    class IRKillCondInstruction : public IIRInstruction
    {
        public:
        IRKillCondInstruction(uint8_t playerId, uint8_t unitId, ConditionComparison comparison, uint32_t quantity) :
            m_PlayerId(playerId), m_UnitId(unitId), m_Comparison(comparison), m_Quantity(quantity), IIRInstruction(IRInstructionType::KillCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("KILLS % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], (int)m_Comparison, (int)m_Quantity);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        ConditionComparison GetComparison() const
        {
            return m_Comparison;
        }

        uint32_t GetQuantity() const
        {
            return m_Quantity;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        ConditionComparison m_Comparison;
        uint32_t m_Quantity;
    };

    class IRDeathCondInstruction : public IIRInstruction
    {
        public:
        IRDeathCondInstruction(uint8_t playerId, uint8_t unitId, ConditionComparison comparison, uint32_t quantity) :
            m_PlayerId(playerId), m_UnitId(unitId), m_Comparison(comparison), m_Quantity(quantity), IIRInstruction(IRInstructionType::DeathCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("DEATHS % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], (int)m_Comparison, (int)m_Quantity);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        ConditionComparison GetComparison() const
        {
            return m_Comparison;
        }

        uint32_t GetQuantity() const
        {
            return m_Quantity;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        ConditionComparison m_Comparison;
        uint32_t m_Quantity;
    };

    class IRCountdownCondInstruction : public IIRInstruction
    {
        public:
        IRCountdownCondInstruction(ConditionComparison comparison, uint32_t time) :
            m_Comparison(comparison), m_Time(time), IIRInstruction(IRInstructionType::CountdownCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("CNTDWN % %", (int)m_Comparison, (int)m_Time);
        }

        ConditionComparison GetComparison() const
        {
            return m_Comparison;
        }

        uint32_t GetTime() const
        {
            return m_Time;
        }

        private:
        ConditionComparison m_Comparison;
        uint32_t m_Time;
    };

    class IRCompilerException : public std::exception
    {
        public:
        IRCompilerException(const char* error) : std::exception(error) {}
        IRCompilerException(const std::string& error) : std::exception(error.c_str()) {}
    };

    class RegisterAliases
    {
        public:
        int GetAlias(const std::string& name) const
        {
            auto it = m_Overrides.find(name);
            if (it != m_Overrides.end())
            {
                return (*it).second;
            }

            for (auto i = 0u; i < m_Aliases.size(); i++)
            {
                if (m_Aliases[i] == name)
                {
                    return (int)Reg_ReservedEnd + i;
                }
            }

            return -1;
        }

        int Allocate(const std::string& name)
        {
            if (m_Aliases.size() >= MAX_REGISTER_INDEX)
            {
                return -1;
            }

            m_Aliases.push_back(name);
            return GetAlias(name);
        }

        void Deallocate(const std::string& name)
        {
            auto it = m_Overrides.find(name);
            if (it != m_Overrides.end())
            {
                m_Overrides.erase(it);
            }

            for (auto it = m_Aliases.begin(); it != m_Aliases.end(); ++it)
            {
                if (*it == name)
                {
                    m_Aliases.erase(it);
                    break;
                }
            }
        }

        void Deallocate(int registerIndex)
        {
            auto index = registerIndex - (int)Reg_ReservedEnd;
            m_Aliases.erase(m_Aliases.begin() + index);
        }

        int GetTemporary()
        {
            m_Aliases.push_back(SafePrintf("temp%", m_Aliases.size()));
            return m_Aliases.size() - 1 + (int)Reg_ReservedEnd;
        }

        void Override(const std::string& name, unsigned int regId)
        {
            m_Overrides.insert(std::make_pair(name, regId));
        }

        private:
        std::vector<std::string> m_Aliases;
        std::unordered_map<std::string, unsigned int> m_Overrides;
    };

    class IRCompiler
    {
        public:
        bool Compile(const std::shared_ptr<IASTNode>& ast);
        void Optimize();

        const std::vector<std::unique_ptr<IIRInstruction>>& GetInstructions() const
        {
            return m_Instructions;
        }

        std::string DumpInstructions(bool lineNumbers) const
        {
            std::string dump;

            for (auto i = 0u; i < m_Instructions.size(); i++)
            {
                if (lineNumbers)
                {
                    dump += SafePrintf("%. ", i);
                }

                dump += m_Instructions[i]->DebugDump();

                if (i < m_Instructions.size() - 1)
                {
                    dump += '\n';
                }
            }

            return dump;
        }

        private:
        void EmitInstruction(IIRInstruction* instruction, std::vector<std::unique_ptr<IIRInstruction>>& instructions)
        {
            instructions.push_back(std::unique_ptr<IIRInstruction>(instruction));
        }

        void EmitFunctionCall(ASTFunctionCall* fnCall, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        void EmitBinaryExpression(ASTBinaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        void EmitUnaryExpression(ASTUnaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        void EmitExpression(IASTNode* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        unsigned int EmitBlockStatement(ASTBlockStatement* blockStatement, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, unsigned int returnReg);
        unsigned int EmitFunction(ASTFunctionDeclaration* fn, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);

        bool IsRegisterName(const std::string& name, RegisterAliases& aliases) const;
        int RegisterNameToIndex(const std::string& name, RegisterAliases& aliases) const;

        int UnitNameToId(const std::string& name) const;
        int PlayerNameToId(const std::string& name) const;

        std::vector<std::unique_ptr<IIRInstruction>> m_Instructions;
        std::unordered_map<std::string, ASTFunctionDeclaration*> m_FunctionDeclarations;
        std::unordered_map<ASTFunctionDeclaration*, unsigned int> m_FunctionIndices;
        std::unordered_map<std::string, unsigned int> m_FunctionStartIndices;
        std::unordered_map<std::string, unsigned int> m_FunctionReturnRegisters;

        unsigned int m_PollEventsAddress = 0;
        unsigned int m_PollEventsRetRegId = 0;
        bool m_HasEvents = false;
    };

}

#endif
