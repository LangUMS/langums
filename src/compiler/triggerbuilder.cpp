#include "triggerbuilder.h"

namespace Langums
{
    unsigned int g_RegistersOwnerPlayer = 7;

    TriggerBuilder::TriggerBuilder(int address, IIRInstruction* instruction, uint8_t playerMask)
        : m_Address(address), m_Instruction(instruction), m_PlayerMask(playerMask)
    {
        using namespace CHK;
        m_Trigger.m_ExecutionFlags = 0;
        m_Trigger.m_ExecutionMask[playerMask - 1] = 1;

        // conditions

        if (address >= 0)
        {
            CodeGen_TestReg(Reg_InstructionCounter, address, TriggerComparisonType::Exactly);
            CodeGen_TestSwitch(Switch_InstructionCounterMutex, false);
        }

        // actions

        CodeGen_PreserveTrigger();
        m_HasChanges = false;
    }

    void TriggerBuilder::CodeGen_TestReg(unsigned int regId, int value, CHK::TriggerComparisonType comparison)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];

        condition.m_Comparison = comparison;
        condition.m_Condition = TriggerConditionType::Deaths;
        condition.m_Quantity = value;
        condition.m_Flags = 16;
        condition.m_Group = g_RegistersOwnerPlayer;
        condition.m_UnitId = regId;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_TestSwitch(unsigned int switchId, bool expectedState)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];

        condition.m_Arg0 = switchId;
        condition.m_Comparison = expectedState ? TriggerComparisonType::SwitchSet : TriggerComparisonType::SwitchCleared;
        condition.m_Condition = TriggerConditionType::Switch;
        condition.m_Flags = 16;
        condition.m_Group = g_RegistersOwnerPlayer;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Always()
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::Always;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Bring(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int locationId, unsigned int quantity)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::Bring;
        condition.m_UnitId = unitId;
        condition.m_Location = locationId + 1;
        condition.m_Quantity = quantity;
        condition.m_Comparison = comparison;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Accumulate(unsigned int playerId, CHK::TriggerComparisonType comparison, CHK::ResourceType resourceType, unsigned int quantity)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::Accumulate;
        condition.m_Arg0 = (uint8_t)resourceType;
        condition.m_Quantity = quantity;
        condition.m_Comparison = comparison;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_ElapsedTime(CHK::TriggerComparisonType comparison, unsigned int quantity)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::ElapsedTime;
        condition.m_Quantity = quantity;
        condition.m_Comparison = comparison;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Commands(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::Command;
        condition.m_UnitId = unitId;
        condition.m_Quantity = quantity;
        condition.m_Comparison = comparison;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Kills(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::Kill;
        condition.m_UnitId = unitId;
        condition.m_Quantity = quantity;
        condition.m_Comparison = comparison;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Deaths(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::Deaths;
        condition.m_UnitId = unitId;
        condition.m_Quantity = quantity;
        condition.m_Comparison = comparison;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Countdown(CHK::TriggerComparisonType comparison, unsigned int time)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::CountdownTimer;
        condition.m_Quantity = time;
        condition.m_Comparison = comparison;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_SetReg(unsigned int regId, int value)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::SetDeaths;
        action.m_Group = g_RegistersOwnerPlayer;
        action.m_Modifier = (uint8_t)TriggerActionState::SetTo;
        action.m_Flags = 16;
        action.m_Arg0 = value;
        action.m_Arg1 = regId;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_IncReg(unsigned int regId, int amount)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::SetDeaths;
        action.m_Group = g_RegistersOwnerPlayer;
        action.m_Modifier = (uint8_t)TriggerActionState::Add;
        action.m_Flags = 16;
        action.m_Arg0 = amount;
        action.m_Arg1 = regId;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_DecReg(unsigned int regId, int amount)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::SetDeaths;
        action.m_Group = g_RegistersOwnerPlayer;
        action.m_Modifier = (uint8_t)TriggerActionState::Subtract;
        action.m_Flags = 16;
        action.m_Arg0 = amount;
        action.m_Arg1 = regId;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_DisplayMsg(unsigned int stringId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::DisplayTextMessage;
        action.m_TriggerText = stringId + 1;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_JumpTo(unsigned int address)
    {
        CodeGen_SetReg(Reg_InstructionCounter, address);
    }

    void TriggerBuilder::CodeGen_PreserveTrigger()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::PreserveTrigger;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Wait(unsigned int milliseconds)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::Wait;
        action.m_Milliseconds = milliseconds;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Comment(unsigned int stringId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::Comment;
        action.m_TriggerText = stringId + 1;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_SetSwitch(unsigned int switchId, CHK::TriggerActionState state)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::SetSwitch;
        action.m_Arg0 = switchId;
        action.m_Modifier = (uint8_t)state;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_CreateUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::CreateUnit;
        action.m_Source = locationId + 1;
        action.m_Group = playerId;
        action.m_Arg1 = unitId;
        action.m_Modifier = quantity;
        action.m_Flags = 4;

        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_KillUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        if (locationId != -1)
        {
            action.m_ActionType = TriggerActionType::KillUnitAtLocation;
            action.m_Source = locationId + 1;
        }
        else
        {
            action.m_ActionType = TriggerActionType::KillUnit;
        }
        
        action.m_Group = playerId;
        action.m_Arg1 = unitId;
        action.m_Modifier = quantity;
        action.m_Flags = 4;

        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_MoveUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int srcLocationId, unsigned int dstLocationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::MoveUnit;
        action.m_Source = srcLocationId + 1;
        action.m_Arg0 = dstLocationId + 1;

        action.m_Group = playerId;
        action.m_Arg1 = unitId;
        action.m_Modifier = quantity;
        action.m_Flags = 4;

        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_MoveLocation(unsigned int playerId, unsigned int unitId, unsigned int srcLocationId, unsigned int dstLocationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::MoveLocation;
        action.m_Source = srcLocationId + 1;
        action.m_Arg0 = dstLocationId + 1;

        action.m_Group = playerId;
        action.m_Arg1 = unitId;
        action.m_Flags = 4;

        m_HasChanges = true;
    }
    
    void TriggerBuilder::CodeGen_Victory()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::Victory;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Defeat()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::Defeat;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Draw()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::Draw;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_CenterView(unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::CenterView;
        action.m_Source = locationId;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_Ping(unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::MinimapPing;
        action.m_Source = locationId;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_SetResources(unsigned int playerId, unsigned int quantity, CHK::TriggerActionState actionType, CHK::ResourceType resourceType)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::SetResources;
        action.m_Arg1 = (uint16_t)resourceType;
        action.m_Group = playerId;
        action.m_Modifier = (uint8_t)actionType;
        action.m_Arg0 = quantity;
        m_HasChanges = true;
    }

    void TriggerBuilder::CodeGen_SetCountdown(unsigned int time, CHK::TriggerActionState actionType)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::SetCountdownTimer;
        action.m_Milliseconds = time;
        action.m_Modifier = (uint8_t)actionType;
        m_HasChanges = true;
    }

}
