#include "triggerbuilder.h"
#include "compiler.h"

namespace Langums
{
    unsigned int g_RegistersOwnerPlayer = 7;
    std::vector<RegisterDef> g_RegisterMap;

    TriggerBuilder::TriggerBuilder(int address, IIRInstruction* instruction, uint8_t playerId)
        : m_Address(address), m_Instruction(instruction), m_PlayerMask(playerId)
    {
        using namespace CHK;
        m_Trigger.m_ExecutionFlags = 0;
        m_Trigger.m_ExecutionMask[playerId - 1] = 1;

        // conditions

        if (address >= 0)
        {
            Cond_TestReg(Reg_InstructionCounter, address, TriggerComparisonType::Exactly);
        }

        // actions

        Action_PreserveTrigger();
        m_HasChanges = false;
    }

    void TriggerBuilder::SetOwner(uint8_t playerId)
    {
        for (auto i = 0; i < 28; i++)
        {
            m_Trigger.m_ExecutionMask[i] = 0;
        }

        m_Trigger.m_ExecutionMask[playerId - 1] = 1;
    }

    void TriggerBuilder::SetExecuteForAllPlayers()
    {
        for (auto i = 0; i < 8; i++)
        {
            m_Trigger.m_ExecutionMask[i] = 1;
        }
    }

    void TriggerBuilder::Cond_TestReg(unsigned int regId, int value, CHK::TriggerComparisonType comparison)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];

        condition.m_Comparison = comparison;
        condition.m_Condition = TriggerConditionType::Deaths;
        condition.m_Quantity = value;
        condition.m_Flags = 16;

        if (regId >= g_RegisterMap.size())
        {
            throw CompilerException("Fatal error. Out of registers.");
        }

        auto& regDef = g_RegisterMap[regId];
        condition.m_Group = regDef.m_PlayerId;
        condition.m_UnitId = regDef.m_Index;
        m_HasChanges = true;
    }

    void TriggerBuilder::Cond_TestSwitch(unsigned int switchId, bool expectedState)
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

    void TriggerBuilder::Cond_Always()
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::Always;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::Cond_Bring(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int locationId, unsigned int quantity)
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

    void TriggerBuilder::Cond_Accumulate(unsigned int playerId, CHK::TriggerComparisonType comparison, CHK::ResourceType resourceType, unsigned int quantity)
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

    void TriggerBuilder::Cond_LeastResources(unsigned int playerId, CHK::ResourceType resourceType)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::LeastResources;
        condition.m_Arg0 = (uint8_t)resourceType;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::Cond_MostResources(unsigned int playerId, CHK::ResourceType resourceType)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::MostResources;
        condition.m_Arg0 = (uint8_t)resourceType;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::Cond_ElapsedTime(CHK::TriggerComparisonType comparison, unsigned int quantity)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::ElapsedTime;
        condition.m_Quantity = quantity;
        condition.m_Comparison = comparison;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::Cond_Commands(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity)
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

    void TriggerBuilder::Cond_CommandsLeast(unsigned int playerId, unsigned int unitId, int locationId)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];

        if (locationId == -1)
        {
            condition.m_Condition = TriggerConditionType::CommandTheLeast;
        }
        else
        {
            condition.m_Condition = TriggerConditionType::CommandTheLeastAt;
            condition.m_Location = locationId + 1;
        }
        condition.m_UnitId = unitId;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::Cond_CommandsMost(unsigned int playerId, unsigned int unitId, int locationId)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];

        if (locationId == -1)
        {
            condition.m_Condition = TriggerConditionType::CommandTheMost;
        }
        else
        {
            condition.m_Condition = TriggerConditionType::CommandsTheMostAt;
            condition.m_Location = locationId + 1;
        }
        condition.m_UnitId = unitId;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::Cond_Kills(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity)
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

    void TriggerBuilder::Cond_KillsLeast(unsigned int playerId, unsigned int unitId)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::LeastKills;
        condition.m_UnitId = unitId;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::Cond_KillsMost(unsigned int playerId, unsigned int unitId)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::MostKills;
        condition.m_UnitId = unitId;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::Cond_Deaths(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity)
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

    void TriggerBuilder::Cond_Countdown(CHK::TriggerComparisonType comparison, unsigned int time)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::CountdownTimer;
        condition.m_Quantity = time;
        condition.m_Comparison = comparison;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::Cond_Opponents(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int quantity)
    {
        using namespace CHK;
        auto& condition = m_Trigger.m_Conditions[m_NextCondition++];
        condition.m_Condition = TriggerConditionType::Opponents;
        condition.m_Quantity = quantity;
        condition.m_Comparison = comparison;
        condition.m_Group = playerId;
        condition.m_Flags = 16;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_SetReg(unsigned int regId, int value)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::SetDeaths;
        action.m_Modifier = (uint8_t)TriggerActionState::SetTo;
        action.m_Flags = 16;

        if (regId >= g_RegisterMap.size())
        {
            throw CompilerException("Fatal error. Out of registers.");
        }

        auto& regDef = g_RegisterMap[regId];
        action.m_Group = regDef.m_PlayerId;
        action.m_Arg0 = value;
        action.m_Arg1 = regDef.m_Index;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_IncReg(unsigned int regId, int amount)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::SetDeaths;
        action.m_Modifier = (uint8_t)TriggerActionState::Add;
        action.m_Flags = 16;

        if (regId >= g_RegisterMap.size())
        {
            throw CompilerException("Fatal error. Out of registers.");
        }

        auto& regDef = g_RegisterMap[regId];
        action.m_Group = regDef.m_PlayerId;
        action.m_Arg0 = amount;
        action.m_Arg1 = regDef.m_Index;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_DecReg(unsigned int regId, int amount)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::SetDeaths;
        action.m_Modifier = (uint8_t)TriggerActionState::Subtract;
        action.m_Flags = 16;

        if (regId >= g_RegisterMap.size())
        {
            throw CompilerException("Fatal error. Out of registers.");
        }

        auto& regDef = g_RegisterMap[regId];
        action.m_Group = regDef.m_PlayerId;
        action.m_Arg0 = amount;
        action.m_Arg1 = regDef.m_Index;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_DisplayMsg(unsigned int stringId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::DisplayTextMessage;
        action.m_TriggerText = stringId + 1;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_JumpTo(unsigned int address)
    {
        Action_SetReg(Reg_InstructionCounter, address);
    }

    void TriggerBuilder::Action_PreserveTrigger()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::PreserveTrigger;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_Wait(unsigned int milliseconds)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::Wait;
        action.m_Milliseconds = milliseconds;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_Comment(unsigned int stringId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::Comment;
        action.m_TriggerText = stringId + 1;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_SetSwitch(unsigned int switchId, CHK::TriggerActionState state)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::SetSwitch;
        action.m_Arg0 = switchId;
        action.m_Modifier = (uint8_t)state;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_CreateUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int locationId)
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

    void TriggerBuilder::Action_KillUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, int locationId)
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

        m_HasChanges = true;
    }

    void TriggerBuilder::Action_RemoveUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        if (locationId != -1)
        {
            action.m_ActionType = TriggerActionType::RemoveUnitAtLocation;
            action.m_Source = locationId + 1;
        }
        else
        {
            action.m_ActionType = TriggerActionType::RemoveUnit;
        }

        action.m_Group = playerId;
        action.m_Arg1 = unitId;
        action.m_Modifier = quantity;
        action.m_Flags = 4;

        m_HasChanges = true;
    }

    void TriggerBuilder::Action_MoveUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int srcLocationId, unsigned int dstLocationId)
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

    void TriggerBuilder::Action_OrderUnit(unsigned int playerId, unsigned int unitId, CHK::TriggerActionState order, unsigned int srcLocationId, unsigned int dstLocationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::Order;
        action.m_Source = srcLocationId + 1;
        action.m_Arg0 = dstLocationId + 1;

        action.m_Group = playerId;
        action.m_Arg1 = unitId;
        action.m_Modifier = (uint8_t)order;
        action.m_Flags = 4;

        m_HasChanges = true;
    }

    void TriggerBuilder::Action_ModifyUnitHP(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::ModifyUnitHitPoints;
        action.m_Source = locationId + 1;

        action.m_Group = playerId;
        action.m_Arg0 = amount;
        action.m_Arg1 = unitId;
        action.m_Modifier = quantity;
        action.m_Flags = 4;

        m_HasChanges = true;
    }

    void TriggerBuilder::Action_ModifyUnitEnergy(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::ModifyUnitEnergy;
        action.m_Source = locationId + 1;

        action.m_Group = playerId;
        action.m_Arg0 = amount;
        action.m_Arg1 = unitId;
        action.m_Modifier = quantity;
        action.m_Flags = 4;

        m_HasChanges = true;
    }

    void TriggerBuilder::Action_ModifyUnitSP(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::ModifyUnitShieldPoints;
        action.m_Source = locationId + 1;

        action.m_Group = playerId;
        action.m_Arg0 = amount;
        action.m_Arg1 = unitId;
        action.m_Modifier = quantity;
        action.m_Flags = 4;

        m_HasChanges = true;
    }

    void TriggerBuilder::Action_ModifyUnitHangar(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::ModifyUnitHangerCount;
        action.m_Source = locationId + 1;

        action.m_Group = playerId;
        action.m_Arg0 = amount;
        action.m_Arg1 = unitId;
        action.m_Modifier = quantity;
        action.m_Flags = 4;

        m_HasChanges = true;
    }

    void TriggerBuilder::Action_GiveUnits(unsigned int srcPlayerId, unsigned int dstPlayerId, unsigned int unitId, unsigned int quantity, unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        action.m_ActionType = TriggerActionType::GiveUnitsToPlayer;
        action.m_Source = locationId + 1;
        action.m_Group = srcPlayerId;
        action.m_Arg0 = dstPlayerId;
        action.m_Arg1 = unitId;
        action.m_Modifier = quantity;
        action.m_Flags = 4;

        m_HasChanges = true;
    }

    void TriggerBuilder::Action_MoveLocation(unsigned int playerId, unsigned int unitId, unsigned int srcLocationId, unsigned int dstLocationId)
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
    
    void TriggerBuilder::Action_Victory()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::Victory;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_Defeat()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::Defeat;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_Draw()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::Draw;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_CenterView(unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::CenterView;
        action.m_Source = locationId;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_Ping(unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::MinimapPing;
        action.m_Source = locationId;
        action.m_Flags = 4;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_SetResources(unsigned int playerId, unsigned int quantity, CHK::TriggerActionState actionType, CHK::ResourceType resourceType)
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

    void TriggerBuilder::Action_SetScore(unsigned int playerId, unsigned int quantity, CHK::TriggerActionState actionType, CHK::ScoreType scoreType)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::SetScore;
        action.m_Arg1 = (uint16_t)scoreType;
        action.m_Group = playerId;
        action.m_Modifier = (uint8_t)actionType;
        action.m_Arg0 = quantity;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_SetDeaths(unsigned int playerId, unsigned int unitId, unsigned int quantity, CHK::TriggerActionState actionType)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::SetDeaths;
        action.m_Arg1 = (uint16_t)unitId;
        action.m_Group = playerId;
        action.m_Modifier = (uint8_t)actionType;
        action.m_Arg0 = quantity;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_SetCountdown(unsigned int time, CHK::TriggerActionState actionType)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::SetCountdownTimer;
        action.m_Milliseconds = time;
        action.m_Modifier = (uint8_t)actionType;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_PauseCountdown()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::PauseTimer;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_UnpauseCountdown()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::UnpauseTimer;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_PauseGame()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::PauseGame;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_UnpauseGame()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::UnpauseGame;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_MuteUnitSpeech()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::MuteUnitSpeech;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_UnmuteUnitSpeech()
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::UnmuteUnitSpeech;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_TalkingPortrait(unsigned int unitId, unsigned int time)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::TalkingPortrait;
        action.m_Milliseconds = time;
        action.m_Arg1 = unitId;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_SetDoodadState(unsigned int playerId, unsigned int unitId, CHK::TriggerActionState state, unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::SetDoodadState;
        action.m_Arg1 = unitId;
        action.m_Group = playerId + 1;
        action.m_Source = locationId + 1;
        action.m_Modifier = (uint8_t)state;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_SetInvincibility(unsigned int playerId, unsigned int unitId, CHK::TriggerActionState state, unsigned int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::SetInvincibility;
        action.m_Arg1 = unitId;
        action.m_Group = playerId + 1;
        action.m_Source = locationId + 1;
        action.m_Modifier = (uint8_t)state;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_RunAIScript(unsigned int playerId, unsigned int scriptName, int locationId)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];

        if (locationId == -1)
        {
            action.m_ActionType = TriggerActionType::RunAIScript;
        }
        else
        {
            action.m_ActionType = TriggerActionType::RunAIScriptAtLocation;
            action.m_Source = locationId + 1;
        }

        action.m_Arg0 = scriptName;
        action.m_Group = playerId + 1;
        m_HasChanges = true;
    }

    void TriggerBuilder::Action_SetAllianceStatus(unsigned int playerId, unsigned int targetPlayerId, AllianceStatus status)
    {
        using namespace CHK;
        auto& action = m_Trigger.m_Actions[m_NextAction++];
        action.m_ActionType = TriggerActionType::SetAllianceStatus;
        action.m_Group = targetPlayerId;
        action.m_Arg1 = (uint16_t)status;
        m_HasChanges = true;
    }

}
