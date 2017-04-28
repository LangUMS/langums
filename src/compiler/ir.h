#ifndef __LANGUMS_IR_H
#define __LANGUMS_IR_H

#include <memory>
#include <unordered_map>
#include <algorithm>

#include "../ast/ast.h"
#include "../libchk/chk.h"
#include "../stringutil.h"

#define MAX_REGISTER_INDEX 224
#define MAX_EVENT_CONDITIONS 64
#define JMP_TO_END_OFFSET_CONSTANT 0xB4DF00D

namespace Langums
{

    using namespace CHK;

    enum ReservedRegisters
    {
        Reg_InstructionCounter = 0,
        Reg_CopyStorage,
        Reg_Temp0,
        Reg_Temp1,
        Reg_Temp2,
        Reg_ReservedEnd,

        Reg_StackTop = 0x574C
    };

    enum ReservedSwitches
    {
        Switch_EventsMutex,
        Switch_ArithmeticUnderflow,
        Switch_Random0,
        Switch_Random1,
        Switch_Random2,
        Switch_Random3,
        Switch_Random4,
        Switch_Random5,
        Switch_Random6,
        Switch_Random7,
        Switch_ReservedEnd
    };

    enum class IRInstructionType
    {
        Nop = 0,
        // core
        Push,           // pushes value on top of the stack and decrements the stack pointer
        Pop,            // pops a value from the stack and increments the stack pointer
        SetReg,         // sets a register to a constant value
        IncReg,         // increments a register by a constant value
        DecReg,         // decrements a register by a constant value
        CopyReg,        // copies a register's value to another register
        Add,            // pops two values off the stack and adds them together, pushes the result on the stack
        Sub,            // pops two values off the stack, subtracts the second from the first, pushes the result on the stack
        Rnd256,         // pushes a random value between 0 and 255 on top of the stack
        Jmp,            // jumps to an instruction using a relative or an absolute offset
        JmpIfEqZero,    // jumps to an instruction if a register is equal to zero
        JmpIfNotEqZero, // jumps to an instruction if a register is not equal to zero
        JmpIfSwNotSet,  // jumps to an instruction if a switch is not set
        JmpIfSwSet,     // jumps to an instruction if a switch is set
        SetSw,          // sets a switch
        // actions
        Wait,           // waits for an amount of time
        DisplayMsg,     // displays a text message
        Spawn,          // spawns a unit
        Kill,           // kills a unit
        Remove,         // removes a unit
        Move,           // moves a unit
        Order,          // orders a unit to move, attack or patrol
        Modify,         // modifies a unit's properties such as hp, energy, shields and hangar count
        Give,           // gives units to another player
        MoveLoc,        // moves a location
        EndGame,        // ends the game in a victory or defeat for a given player
        CenterView,     // centers the camera on a location for a given player
        Ping,           // Minimap ping at a location
        SetResource,    // sets the resource count for a given resource for a given player
        IncResource,    // increments the resource count for a given resource for a given player
        DecResource,    // decrements the resource count for a given resource for a given player
        SetScore,       // sets the score for a player
        IncScore,       // increments the score for a player
        DecScore,       // decrements the score for a player
        SetCountdown,   // sets the countdown timer,
        PauseCountdown, // pauses/ unpauses the countdown timer
        MuteUnitSpeech, // mutes/ unmutes unit speech
        SetDeaths,      // sets the death count for a given unit
        IncDeaths,      // increments the death count for a given unit
        DecDeaths,      // decrements the death count for a given unit
        Talk,           // shows the unit talking portrait for an amount of time
        SetDoodad,      // sets the state of a doodad
        SetInvincible,  // sets invincibility for units
        AIScript,       // runs an ai script
        SetAlly,        // sets alliance status between two players
        SetObj,         // sets the mission objectives
        // conditions
        Event,
        BringCond,      // Bring trigger condition
        AccumCond,      // Accumulate trigger condition
        LeastResCond,   // Player has the least quantity of a resource 
        MostResCond,    // Player has the most quantity of a resource
        ScoreCond,      // Score condition
        HiScoreCond,    // Highest score conditiion
        LowScoreCond,   // Lowest score condition
        TimeCond,       // Elapsed time trigger condition
        CmdCond,        // Player commands a quantity of units condition
        CmdLeastCond,   // Player commands the least quantity of units condition
        CmdMostCond,    // Player commands the most quantity of units condition
        KillCond,       // Player has killed a quantity of units condition
        KillLeastCond,  // Player has the least units killed
        KillMostCond,   // Player hast the most units killed
        DeathCond,      // Player has lost a quantity of units condition
        CountdownCond,  // Countdown timer condition
        OpponentsCond,  // Opponents condition
        // unit properties
        Unit,
        UnitProp
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
        else if (regId >= Reg_StackTop)
        {
            return SafePrintf("[STACK %]", regId - Reg_StackTop);
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
        IRInstructionType m_Type = IRInstructionType::Nop;
    };

    class IRNopInstruction : public IIRInstruction
    {
        public:
        IRNopInstruction() : IIRInstruction(IRInstructionType::Nop) {}

        std::string DebugDump() const
        {
            return "NOP";
        }
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

        int GetRegisterId() const
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

    class IRRnd256Instruction : public IIRInstruction
    {
        public:
        IRRnd256Instruction() : IIRInstruction(IRInstructionType::Rnd256) {}

        std::string DebugDump() const
        {
            return "RND256";
        }
    };

    class IRDisplayMsgInstruction : public IIRInstruction
    {
        public:
        IRDisplayMsgInstruction(const std::string& message, int playerId) :
            m_Message(message), m_PlayerId(playerId), IIRInstruction(IRInstructionType::DisplayMsg) {}

        const std::string& GetMessage() const
        {
            return m_Message;
        }

        int GetPlayerId() const
        {
            return m_PlayerId;
        }

        std::string DebugDump() const
        {
            auto msg = m_Message;
            std::replace(msg.begin(), msg.end(), '\n', ' ');

            if (m_PlayerId == -1)
            {
                return SafePrintf("MSG \"%\" [ALL]", msg);
            }

            return SafePrintf("MSG \"%\" %", msg, m_PlayerId);
        }

        private:
        std::string m_Message;
        int m_PlayerId;
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

    class IRSpawnInstruction : public IIRInstruction
    {
        public:
        IRSpawnInstruction(uint8_t playerId, uint8_t unitId, uint32_t regId, bool isValueLiteral, const std::string& locationName, int propsSlot) :
            m_PlayerId(playerId), m_UnitId(unitId), m_RegId(regId), m_IsLiteralValue(isValueLiteral),
            m_LocationName(locationName), m_PropsSlot(propsSlot), IIRInstruction(IRInstructionType::Spawn) {}

        std::string DebugDump() const
        {
            std::string props;
            if (m_PropsSlot != -1)
            {
                props = SafePrintf("[SLOT %]", m_PropsSlot);
            }

            if (m_IsLiteralValue)
            {
                return SafePrintf("SPAWN % % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], m_RegId, m_LocationName, props);
            }

            return SafePrintf("SPAWN % % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], RegisterIdToString(m_RegId), m_LocationName, props);
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

        int GetPropsSlot() const
        {
            return m_PropsSlot;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        uint32_t m_RegId;
        bool m_IsLiteralValue = false;
        std::string m_LocationName;
        int m_PropsSlot;
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

    class IRRemoveInstruction : public IIRInstruction
    {
        public:
        IRRemoveInstruction(uint8_t playerId, uint8_t unitId, uint32_t regId, bool isValueLiteral, const std::string& locationName) :
            m_PlayerId(playerId), m_UnitId(unitId), m_RegId(regId), m_IsLiteralValue(isValueLiteral), m_LocationName(locationName),
            IIRInstruction(IRInstructionType::Remove) {}

        std::string DebugDump() const
        {
            if (m_IsLiteralValue)
            {
                return SafePrintf("REMOVE % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], m_RegId, m_LocationName);
            }

            return SafePrintf("REMOVE % % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], RegisterIdToString(m_RegId), m_LocationName);
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

    class IRGiveInstruction : public IIRInstruction
    {
        public:
        IRGiveInstruction(uint8_t srcPlayerId, uint8_t dstPlayerId, uint8_t unitId, uint32_t regId, bool isValueLiteral, const std::string& locationName) :
            m_SrcPlayerId(srcPlayerId), m_DstPlayerId(dstPlayerId), m_UnitId(unitId), m_RegId(regId), m_IsLiteralValue(isValueLiteral), m_LocationName(locationName),
            IIRInstruction(IRInstructionType::Give) {}

        std::string DebugDump() const
        {
            if (m_IsLiteralValue)
            {
                return SafePrintf("GIVE % % % % %", PlayersByName[m_SrcPlayerId], PlayersByName[m_DstPlayerId], UnitsByName[m_UnitId], m_RegId, m_LocationName);
            }

            return SafePrintf("GIVE % % % % %", PlayersByName[m_SrcPlayerId], PlayersByName[m_DstPlayerId], UnitsByName[m_UnitId], RegisterIdToString(m_RegId), m_LocationName);
        }

        uint8_t GetSrcPlayerId() const
        {
            return m_SrcPlayerId;
        }

        uint8_t GetDstPlayerId() const
        {
            return m_DstPlayerId;
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
        uint8_t m_SrcPlayerId;
        uint8_t m_DstPlayerId;
        uint8_t m_UnitId;
        uint32_t m_RegId;
        bool m_IsLiteralValue = false;
        std::string m_LocationName;
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

    class IRSetScoreInstruction : public IIRInstruction
    {
        public:
        IRSetScoreInstruction(uint8_t playerId, ScoreType scoreType, uint32_t regId, bool isLiteralValue) :
            m_PlayerId(playerId), m_ScoreType(scoreType), m_RegId(regId), m_IsValueLiteral(isLiteralValue),
            IIRInstruction(IRInstructionType::SetScore) {}

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

            return SafePrintf("SETSCORE % % %", PlayersByName[m_PlayerId], (int)m_ScoreType, qty);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ScoreType GetScoreType() const
        {
            return m_ScoreType;
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
        ScoreType m_ScoreType;
        uint32_t m_RegId;
        bool m_IsValueLiteral;
    };

    class IRIncScoreInstruction : public IIRInstruction
    {
        public:
        IRIncScoreInstruction(uint8_t playerId, ScoreType scoreType, uint32_t regId, bool isLiteralValue) :
            m_PlayerId(playerId), m_ScoreType(scoreType), m_RegId(regId), m_IsValueLiteral(isLiteralValue),
            IIRInstruction(IRInstructionType::IncScore) {}

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

            return SafePrintf("INCSCORE % % %", PlayersByName[m_PlayerId], (int)m_ScoreType, qty);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ScoreType GetScoreType() const
        {
            return m_ScoreType;
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
        ScoreType m_ScoreType;
        uint32_t m_RegId;
        bool m_IsValueLiteral;
    };

    class IRDecScoreInstruction : public IIRInstruction
    {
        public:
        IRDecScoreInstruction(uint8_t playerId, ScoreType scoreType, uint32_t regId, bool isLiteralValue) :
            m_PlayerId(playerId), m_ScoreType(scoreType), m_RegId(regId), m_IsValueLiteral(isLiteralValue),
            IIRInstruction(IRInstructionType::DecScore) {}

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

            return SafePrintf("DECSCORE % % %", PlayersByName[m_PlayerId], (int)m_ScoreType, qty);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ScoreType GetScoreType() const
        {
            return m_ScoreType;
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
        ScoreType m_ScoreType;
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

    class IRPauseCountdownInstruction : public IIRInstruction
    {
        public:
        IRPauseCountdownInstruction(bool unpause) :
            m_Unpause(unpause), IIRInstruction(IRInstructionType::PauseCountdown) {}

        std::string DebugDump() const
        {
            return SafePrintf("SETCNDWN %", m_Unpause ? "[UNPAUSE]" : "[PAUSE]");
        }

        bool IsUnpause() const
        {
            return m_Unpause;
        }

        private:
        bool m_Unpause;
    };

    class IRMuteUnitSpeechInstruction : public IIRInstruction
    {
        public:
        IRMuteUnitSpeechInstruction(bool unmute) :
            m_Unmute(unmute), IIRInstruction(IRInstructionType::MuteUnitSpeech) {}

        std::string DebugDump() const
        {
            if (m_Unmute)
            {
                return "UNMUTE";
            }
            else
            {
                return "MUTE";
            }
        }

        bool IsUnmute() const
        {
            return m_Unmute;
        }

        private:
        bool m_Unmute;
    };

    class IRSetDeathsInstruction : public IIRInstruction
    {
        public:
        IRSetDeathsInstruction(unsigned int playerId, unsigned int unitId, unsigned int regId, bool isLiteralValue) :
            m_PlayerId(playerId), m_UnitId(unitId), m_RegisterId(regId), m_IsValueLiteral(isLiteralValue), IIRInstruction(IRInstructionType::SetDeaths) {}

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
            return m_RegisterId;
        }

        bool IsValueLiteral() const
        {
            return m_IsValueLiteral;
        }

        std::string DebugDump() const
        {
            if (m_IsValueLiteral)
            {
                return SafePrintf("SETDEATHS % % %", CHK::PlayersByName[m_PlayerId], CHK::UnitsByName[m_UnitId], m_RegisterId);
            }

            return SafePrintf("SETDEATHS % % %", CHK::PlayersByName[m_PlayerId], CHK::UnitsByName[m_UnitId], RegisterIdToString(m_RegisterId));
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        uint32_t m_RegisterId;
        bool m_IsValueLiteral;
    };

    class IRIncDeathsInstruction : public IIRInstruction
    {
        public:
        IRIncDeathsInstruction(unsigned int playerId, unsigned int unitId, unsigned int regId, bool isLiteralValue) :
            m_PlayerId(playerId), m_UnitId(unitId), m_RegisterId(regId), m_IsValueLiteral(isLiteralValue), IIRInstruction(IRInstructionType::IncDeaths) {}

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
            return m_RegisterId;
        }

        bool IsValueLiteral() const
        {
            return m_IsValueLiteral;
        }

        std::string DebugDump() const
        {
            if (m_IsValueLiteral)
            {
                return SafePrintf("INCDEATHS % % %", CHK::PlayersByName[m_PlayerId], CHK::UnitsByName[m_UnitId], m_RegisterId);
            }

            return SafePrintf("INCDEATHS % % %", CHK::PlayersByName[m_PlayerId], CHK::UnitsByName[m_UnitId], RegisterIdToString(m_RegisterId));
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        uint32_t m_RegisterId;
        bool m_IsValueLiteral;
    };

    class IRDecDeathsInstruction : public IIRInstruction
    {
        public:
           IRDecDeathsInstruction(unsigned int playerId, unsigned int unitId, unsigned int regId, bool isLiteralValue) :
            m_PlayerId(playerId), m_UnitId(unitId), m_RegisterId(regId), m_IsValueLiteral(isLiteralValue), IIRInstruction(IRInstructionType::DecDeaths) {}

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
            return m_RegisterId;
        }

        bool IsValueLiteral() const
        {
            return m_IsValueLiteral;
        }

        std::string DebugDump() const
        {
            if (m_IsValueLiteral)
            {
                return SafePrintf("INCDEATHS % % %", CHK::PlayersByName[m_PlayerId], CHK::UnitsByName[m_UnitId], m_RegisterId);
            }

            return SafePrintf("INCDEATHS % % %", CHK::PlayersByName[m_PlayerId], CHK::UnitsByName[m_UnitId], RegisterIdToString(m_RegisterId));
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        uint32_t m_RegisterId;
        bool m_IsValueLiteral;
    };

    class IRTalkInstruction : public IIRInstruction
    {
        public:
        IRTalkInstruction(uint8_t playerId, unsigned int unitId, unsigned int time) :
            m_PlayerId(playerId), m_UnitId(unitId), m_Time(time), IIRInstruction(IRInstructionType::Talk) {}

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        uint32_t GetTime() const
        {
            return m_Time;
        }

        std::string DebugDump() const
        {
            return SafePrintf("TALK % % %", CHK::PlayersByName[m_PlayerId], CHK::UnitsByName[m_UnitId], m_Time);
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        uint32_t m_Time;
    };

    class IRSetDoodadInstruction : public IIRInstruction
    {
        public:
        IRSetDoodadInstruction(uint8_t playerId, unsigned int unitId, const std::string& locationName, CHK::TriggerActionState state) :
            m_PlayerId(playerId), m_UnitId(unitId), m_LocationName(locationName), m_State(state), IIRInstruction(IRInstructionType::SetDoodad) {}

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

        CHK::TriggerActionState GetState() const
        {
            return m_State;
        }

        std::string DebugDump() const
        {
            return SafePrintf("SETDOODAD % % % %", CHK::PlayersByName[m_PlayerId], CHK::UnitsByName[m_UnitId], m_LocationName, (int)m_State);
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        std::string m_LocationName;
        CHK::TriggerActionState m_State;
    };

    class IRSetInvincibleInstruction : public IIRInstruction
    {
        public:
        IRSetInvincibleInstruction(uint8_t playerId, unsigned int unitId, const std::string& locationName, CHK::TriggerActionState state) :
            m_PlayerId(playerId), m_UnitId(unitId), m_LocationName(locationName), m_State(state), IIRInstruction(IRInstructionType::SetInvincible) {}

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

        CHK::TriggerActionState GetState() const
        {
            return m_State;
        }

        std::string DebugDump() const
        {
            return SafePrintf("SETINVINC % % % %", CHK::PlayersByName[m_PlayerId], CHK::UnitsByName[m_UnitId], m_LocationName, (int)m_State);
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        std::string m_LocationName;
        CHK::TriggerActionState m_State;
    };

    class IRAIScriptInstruction : public IIRInstruction
    {
        public:
        IRAIScriptInstruction(uint8_t playerId, unsigned int scriptName, const std::string& locationName) :
            m_PlayerId(playerId), m_ScriptName(scriptName), m_LocationName(locationName), IIRInstruction(IRInstructionType::AIScript) {}
        
        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        unsigned int GetScriptName() const
        {
            return m_ScriptName;
        }

        const std::string& GetLocationName() const
        {
            return m_LocationName;
        }

        std::string DebugDump() const
        {
            std::string ai((char*)&m_ScriptName, 4);
            return SafePrintf("AISCRIPT % % %", CHK::PlayersByName[m_PlayerId], ai, m_LocationName);
        }

        private:
        uint8_t m_PlayerId;
        uint32_t m_ScriptName;
        std::string m_LocationName;
    };

    enum class AllianceStatus
    {
        Enemy = 0,
        Ally,
        AlliedVictory
    };

    class IRSetAllyInstruction : public IIRInstruction
    {
        public:
        IRSetAllyInstruction(uint8_t playerId, uint8_t targetPlayerId, AllianceStatus status) :
            m_PlayerId(playerId), m_TargetPlayerId(targetPlayerId), m_Status(status), IIRInstruction(IRInstructionType::SetAlly) {}

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetTargetPlayerId() const
        {
            return m_TargetPlayerId;
        }

        AllianceStatus GetAllianceStatus() const
        {
            return m_Status;
        }

        std::string DebugDump() const
        {
            std::string status;
            switch (m_Status)
            {
            case AllianceStatus::Ally:
                status = "[ALLY]";
                break;
            case AllianceStatus::Enemy:
                status = "[ENEMY]";
                break;
            case AllianceStatus::AlliedVictory:
                status = "[ALLIED VICTORY]";
                break;
            }

            return SafePrintf("SETALLY % % %", CHK::PlayersByName[m_PlayerId], CHK::PlayersByName[m_TargetPlayerId], status);
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_TargetPlayerId;
        AllianceStatus m_Status;
    };

    class IRSetObjInstruction : public IIRInstruction
    {
        public:
        IRSetObjInstruction(const std::string& text) :
            m_Text(text), IIRInstruction(IRInstructionType::SetObj) {}

        const std::string& GetText() const
        {
            return m_Text;
        }

        std::string DebugDump() const
        {
            auto msg = m_Text;
            std::replace(msg.begin(), msg.end(), '\n', ' ');

            return SafePrintf("SETOBJ %", msg);
        }

        private:
        std::string m_Text;
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

    class IRLeastResCondInstruction : public IIRInstruction
    {
        public:
        IRLeastResCondInstruction(uint8_t playerId, ResourceType resourceType) :
            m_PlayerId(playerId), m_ResourceType(resourceType), IIRInstruction(IRInstructionType::LeastResCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("LEASTRES % %", PlayersByName[m_PlayerId], m_ResourceType == ResourceType::Ore ? "Minerals" : "Gas");
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ResourceType GetResourceType() const
        {
            return m_ResourceType;
        }

        private:
        uint8_t m_PlayerId;
        ResourceType m_ResourceType;
    };

    class IRMostResCondInstruction : public IIRInstruction
    {
        public:
        IRMostResCondInstruction(uint8_t playerId, ResourceType resourceType) :
            m_PlayerId(playerId), m_ResourceType(resourceType), IIRInstruction(IRInstructionType::MostResCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("MOSTRES % %", PlayersByName[m_PlayerId], m_ResourceType == ResourceType::Ore ? "Minerals" : "Gas");
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ResourceType GetResourceType() const
        {
            return m_ResourceType;
        }

        private:
        uint8_t m_PlayerId;
        ResourceType m_ResourceType;
    };

    class IRHiScoreCondInstruction : public IIRInstruction
    {
        public:
        IRHiScoreCondInstruction(uint8_t playerId, ScoreType scoreType) :
            m_PlayerId(playerId), m_ScoreType(scoreType), IIRInstruction(IRInstructionType::HiScoreCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("HISCORE % %", PlayersByName[m_PlayerId], (int)m_ScoreType);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ScoreType GetScoreType() const
        {
            return m_ScoreType;
        }

        private:
        uint8_t m_PlayerId;
        ScoreType m_ScoreType;
    };

    class IRLowScoreCondInstruction : public IIRInstruction
    {
        public:
        IRLowScoreCondInstruction(uint8_t playerId, ScoreType scoreType) :
            m_PlayerId(playerId), m_ScoreType(scoreType), IIRInstruction(IRInstructionType::LowScoreCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("LOWSCORE % %", PlayersByName[m_PlayerId], (int)m_ScoreType);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ScoreType GetScoreType() const
        {
            return m_ScoreType;
        }

        private:
        uint8_t m_PlayerId;
        ScoreType m_ScoreType;
    };

    class IRScoreCondInstruction : public IIRInstruction
    {
        public:
        IRScoreCondInstruction(uint8_t playerId, ScoreType scoreType, ConditionComparison comparison, uint32_t quantity) :
            m_PlayerId(playerId), m_ScoreType(scoreType), m_Comparison(comparison), m_Quantity(quantity), IIRInstruction(IRInstructionType::ScoreCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("SCORE % % % %", PlayersByName[m_PlayerId], (int)m_ScoreType, (int)m_Comparison, (int)m_Quantity);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        ScoreType GetScoreType() const
        {
            return m_ScoreType;
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
        ScoreType m_ScoreType;
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

    class IRCmdLeastCondInstruction : public IIRInstruction
    {
        public:
        IRCmdLeastCondInstruction(uint8_t playerId, uint8_t unitId, const std::string& locationName) :
            m_PlayerId(playerId), m_UnitId(unitId), m_LocationName(locationName), IIRInstruction(IRInstructionType::CmdLeastCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("CMDLEAST % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], m_LocationName);
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

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        std::string m_LocationName; // location name can be empty which means "no location"
    };

    class IRCmdMostCondInstruction : public IIRInstruction
    {
        public:
        IRCmdMostCondInstruction(uint8_t playerId, uint8_t unitId, const std::string& locationName) :
            m_PlayerId(playerId), m_UnitId(unitId), m_LocationName(locationName), IIRInstruction(IRInstructionType::CmdMostCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("CMDMOST % % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId], m_LocationName);
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

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
        std::string m_LocationName; // location name can be empty which means "no location"
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

    class IRKillLeastCondInstruction : public IIRInstruction
    {
        public:
        IRKillLeastCondInstruction(uint8_t playerId, uint8_t unitId) :
            m_PlayerId(playerId), m_UnitId(unitId), IIRInstruction(IRInstructionType::KillLeastCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("LEASTKILLS % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId]);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
    };

    class IRKillMostCondInstruction : public IIRInstruction
    {
        public:
        IRKillMostCondInstruction(uint8_t playerId, uint8_t unitId) :
            m_PlayerId(playerId), m_UnitId(unitId), IIRInstruction(IRInstructionType::KillMostCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("MOSTKILLS % %", PlayersByName[m_PlayerId], UnitsByName[m_UnitId]);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
        }

        uint8_t GetUnitId() const
        {
            return m_UnitId;
        }

        private:
        uint8_t m_PlayerId;
        uint8_t m_UnitId;
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

    class IROpponentsCondInstruction : public IIRInstruction
    {
        public:
        IROpponentsCondInstruction(uint8_t playerId, ConditionComparison comparison, uint32_t quantity) :
            m_PlayerId(playerId), m_Comparison(comparison), m_Quantity(quantity), IIRInstruction(IRInstructionType::OpponentsCond) {}

        std::string DebugDump() const
        {
            return SafePrintf("OPPONENTS % % %", PlayersByName[m_PlayerId], (int)m_Comparison, m_Quantity);
        }

        uint8_t GetPlayerId() const
        {
            return m_PlayerId;
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
        ConditionComparison m_Comparison;
        uint32_t m_Quantity;
    };

    class IRUnitInstruction : public IIRInstruction
    {
        public:
        IRUnitInstruction(unsigned int propertyCount) :
            m_PropertyCount(propertyCount), IIRInstruction(IRInstructionType::Unit) {}

        std::string DebugDump() const
        {
            return SafePrintf("UNIT %", m_PropertyCount);
        }

        unsigned int GetPropertyCount() const
        {
            return m_PropertyCount;
        }

        private:
        unsigned int m_PropertyCount;
    };

    enum class UnitPropType
    {
        HitPoints = 0,
        ShieldPoints,
        Energy,
        ResourceAmount,
        HangarCount,
        Cloaked,
        Burrowed,
        InTransit,
        Hallucinated,
        Invincible
    };

    class IRUnitPropInstruction : public IIRInstruction
    {
        public:
        IRUnitPropInstruction(UnitPropType propType, unsigned int value) :
            m_PropType(propType), m_Value(value), IIRInstruction(IRInstructionType::UnitProp) {}

        std::string DebugDump() const
        {
            std::string type;

            switch (m_PropType)
            {
            case UnitPropType::HitPoints:
                type = "HitPoints";
                break;
            case UnitPropType::ShieldPoints:
                type = "ShieldPoints";
                break;
            case UnitPropType::Energy:
                type = "Energy";
                break;
            case UnitPropType::ResourceAmount:
                type = "ResourceAmount";
                break;
            case UnitPropType::HangarCount:
                type = "HangarCount";
                break;
            case UnitPropType::Cloaked:
                type = "Cloaked";
                break;
            case UnitPropType::Burrowed:
                type = "Burrowed";
                break;
            case UnitPropType::InTransit:
                type = "InTransit";
                break;
            case UnitPropType::Hallucinated:
                type = "Hallucinated";
                break;
            case UnitPropType::Invincible:
                type = "Invincible";
                break;
            }

            return SafePrintf("PROP % %", type, m_Value);
        }

        unsigned int GetValue() const
        {
            return m_Value;
        }

        UnitPropType GetPropType() const
        {
            return m_PropType;
        }

        private:
        UnitPropType m_PropType;
        unsigned int m_Value;
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
        RegisterAliases() {}

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

        void EmitFunctionCall(ASTFunctionCall* fnCall, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool ignoreReturnValue);
        void EmitBinaryExpression(ASTBinaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        void EmitUnaryExpression(ASTUnaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        void EmitExpression(IASTNode* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        unsigned int EmitBlockStatement(ASTBlockStatement* blockStatement, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        unsigned int EmitFunction(ASTFunctionDeclaration* fn, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);

        bool IsRegisterName(const std::string& name, RegisterAliases& aliases) const;
        int RegisterNameToIndex(const std::string& name, RegisterAliases& aliases) const;

        int UnitNameToId(const std::string& name) const;
        int PlayerNameToId(const std::string& name) const;

        uint8_t ParsePlayerIdArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        ConditionComparison ParseComparisonArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        int ParseQuantityArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        uint8_t ParseUnitTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        uint32_t ParseAIScriptArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        std::string ParseLocationArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        CHK::ResourceType ParseResourceTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        CHK::ScoreType ParseScoreTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        AllianceStatus ParseAllianceStatusArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        EndGameType ParseEndGameCondition(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        CHK::TriggerActionState ParseOrderType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        CHK::TriggerActionState ParseToggleState(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        ModifyType ParseModifyType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        UnitPropType ParseUnitPropType(const std::string& propName);

        int ParseQuantityExpression(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex,
            std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool& isLiteral);

        std::vector<std::unique_ptr<IIRInstruction>> m_Instructions;
        std::unordered_map<std::string, ASTFunctionDeclaration*> m_FunctionDeclarations;
        std::unordered_map<ASTFunctionDeclaration*, unsigned int> m_FunctionIndices;

        std::unordered_map<std::string, unsigned int> m_UnitProperties;

        RegisterAliases m_GlobalAliases;

        std::vector<std::unique_ptr<IIRInstruction>> m_PollEventsInstructions;
        bool m_HasEvents = false;
    };

}

#endif
