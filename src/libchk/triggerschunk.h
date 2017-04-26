#ifndef __LIBCHK_TRIGGERSCHUNK_H
#define __LIBCHK_TRIGGERSCHUNK_H

#include "ichunk.h"
#include "enums.h"

namespace CHK
{

    enum class TriggerConditionFlag : uint8_t
    {
        Unknown0 = (1 << 0),
        Disabled = (1 << 1),
        AlwaysDisplay = (1 << 2),
        UnitPropertiesUsed = (1 << 3),
        UnitTypeUsed = (1 << 4),
        UnitIdUsed = (1 << 5),
        Unknown1 = (1 << 6)
    };

    struct TriggerCondition
    {
        uint32_t m_Location = 0; // 1 based, 0 - no location
        uint32_t m_Group = 0;
        uint32_t m_Quantity = 0; // how many/ resource amount
        uint16_t m_UnitId = 0;
        TriggerComparisonType m_Comparison = TriggerComparisonType::AtLeast; // numeric comparison, switch state
        TriggerConditionType m_Condition = TriggerConditionType::NoCondition; // condition
        uint8_t m_Arg0 = 0; // resource type, score type, switch number (0 based)
        uint8_t m_Flags = 0;
        uint16_t m_Unused = 0 ;
    };

    enum class TriggerActionFlag : uint8_t
    {
        IgnoreWaitOnce = (1 << 0),
        Disabled = (1 << 1),
        AlwaysDisplay = (1 << 2),
        UnitPropertiesUsed = (1 << 3),
        UnitTypeUsed = (1 << 4),
        UnitIdUsed = (1 << 5),
        Unknown0 = (1 << 6)
    };

    struct TriggerAction
    {
        uint32_t m_Source = 0; // source or only location (1 based, 0 - no location)
        uint32_t m_TriggerText = 0; // string index for trigger name, 0 - no string
        uint32_t m_WAVStringIndex = 0; // 0 - no string
        uint32_t m_Milliseconds = 0;
        uint32_t m_Group = 0; // group/ player affected
        uint32_t m_Arg0 = 0; // second group affected, destination location, CUWP, AI script, switch (0 based)
        uint16_t m_Arg1 = 0; // unit type, score type, resource type, alliance status
        TriggerActionType m_ActionType = TriggerActionType::NoAction;
        uint8_t m_Modifier = 0; // number of units (0 - all units), action state, unit order, number modifier
        uint8_t m_Flags = 0;
        uint8_t m_Unused0 = 0;
        uint8_t m_Unused1 = 0;
        uint8_t m_Unused2 = 0;
    };

    struct Trigger
    {
        TriggerCondition m_Conditions[16];
        TriggerAction m_Actions[64];
        uint32_t m_ExecutionFlags = 0;
        uint8_t m_ExecutionMask[28] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    };

    class CHKTriggersChunk : public IChunk
    {
        public:
        CHKTriggersChunk(const std::vector<char>& data, const std::string& type) : IChunk(type)
        {
            SetBytes(data);
        }

        size_t GetTriggersCount() const
        {
            return m_TriggersCount;
        }

        Trigger* GetTrigger(size_t index) const
        {
            if (index >= m_TriggersCount)
            {
                return nullptr;
            }

            auto& bytes = GetBytes();

            return (Trigger*)(bytes.data() + sizeof(Trigger) * index);
        }

        virtual void SetBytes(const std::vector<char>& data)
        {
            m_TriggersCount = data.size() / 2400;
            IChunk::SetBytes(data);
        }

        private:
        size_t m_TriggersCount = 0;
    };

}

#endif
