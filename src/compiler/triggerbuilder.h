#ifndef __LANGUMS_TRIGGERBUILDER_H
#define __LANGUMS_TRIGGERBUILDER_H

#include "ir.h"

namespace Langums
{

    struct RegisterDef
    {
        uint8_t m_PlayerId;
        uint8_t m_Index;
    };

    extern std::vector<RegisterDef> g_RegisterMap;

    class TriggerBuilder
    {
        public:
        TriggerBuilder(int address, IIRInstruction* instruction, uint8_t playerId);

        void SetOwner(uint8_t playerId);
        void SetExecuteForAllPlayers();

        void Cond_TestReg(unsigned int regId, int value, CHK::TriggerComparisonType comparison);
        void Cond_TestSwitch(unsigned int switchId, bool expectedState);
        void Cond_Always();
        void Cond_Bring(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int locationId, unsigned int quantity);
        void Cond_Accumulate(unsigned int playerId, CHK::TriggerComparisonType comparison, CHK::ResourceType resourceType, unsigned int quantity);
        void Cond_LeastResources(unsigned int playerId, CHK::ResourceType resourceType);
        void Cond_MostResources(unsigned int playerId, CHK::ResourceType resourceType);
        void Cond_Score(unsigned int playerId, CHK::TriggerComparisonType comparison, CHK::ScoreType scoreType, unsigned int quantity);
        void Cond_LowestScore(unsigned int playerId, CHK::ScoreType scoreType);
        void Cond_HighestScore(unsigned int playerId, CHK::ScoreType scoreType);
        void Cond_ElapsedTime(CHK::TriggerComparisonType comparison, unsigned int quantity);
        void Cond_Commands(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity);
        void Cond_CommandsLeast(unsigned int playerId, unsigned int unitId, int locationId);
        void Cond_CommandsMost(unsigned int playerId,  unsigned int unitId, int locationId);
        void Cond_Kills(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity);
        void Cond_KillsLeast(unsigned int playerId, unsigned int unitId);
        void Cond_KillsMost(unsigned int playerId, unsigned int unitId);
        void Cond_Deaths(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity);
        void Cond_Countdown(CHK::TriggerComparisonType comparison, unsigned int time);
        void Cond_Opponents(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int quantity);

        void Action_SetReg(unsigned int regId, int value);
        void Action_IncReg(unsigned int regId, int amount);
        void Action_DecReg(unsigned int regId, int amount);
        void Action_DisplayMsg(unsigned int stringId);
        void Action_SetMissionObjectives(unsigned int stringId);
        void Action_JumpTo(unsigned int address);
        void Action_PreserveTrigger();
        void Action_Wait(unsigned int milliseconds);
        void Action_Comment(unsigned int stringId);
        void Action_SetSwitch(unsigned int switchId, CHK::TriggerActionState state);
        void Action_CreateUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int locationId, int propsSlot);
        void Action_KillUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, int locationId);
        void Action_RemoveUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, int locationId);
        void Action_MoveUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int srcLocationId, unsigned int dstLocationId);
        void Action_OrderUnit(unsigned int playerId, unsigned int unitId, CHK::TriggerActionState order, unsigned int srcLocationId, unsigned int dstLocationId);
        void Action_ModifyUnitHP(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId);
        void Action_ModifyUnitEnergy(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId);
        void Action_ModifyUnitSP(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId);
        void Action_ModifyUnitHangar(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId);
        void Action_GiveUnits(unsigned int srcPlayerId, unsigned int dstPlayerId, unsigned int unitId, unsigned int quantity, unsigned int locationId);
        void Action_MoveLocation(unsigned int playerId, unsigned int unitId, unsigned int srcLocationId, unsigned int dstLocationId);
        void Action_Victory();
        void Action_Defeat();
        void Action_Draw();
        void Action_CenterView(unsigned int locationId);
        void Action_Ping(unsigned int locationId);
        void Action_SetResources(unsigned int playerId, unsigned int quantity, CHK::TriggerActionState action, CHK::ResourceType resourceType);
        void Action_SetScore(unsigned int playerId, unsigned int quantity, CHK::TriggerActionState action, CHK::ScoreType scoreType);
        void Action_SetDeaths(unsigned int playerId, unsigned int unitId, unsigned int quantity, CHK::TriggerActionState action);
        void Action_SetCountdown(unsigned int time, CHK::TriggerActionState action);
        void Action_PauseCountdown();
        void Action_UnpauseCountdown();
        void Action_PauseGame();
        void Action_UnpauseGame();
        void Action_SetNextScenario(unsigned int stringId);
        void Action_MuteUnitSpeech();
        void Action_UnmuteUnitSpeech();
        void Action_TalkingPortrait(unsigned int unitId, unsigned int time);
        void Action_SetDoodadState(unsigned int playerId, unsigned int unitId, CHK::TriggerActionState state, unsigned int locationId);
        void Action_SetInvincibility(unsigned int playerId, unsigned int unitId, CHK::TriggerActionState state, unsigned int locationId);
        void Action_RunAIScript(unsigned int playerId, unsigned int scriptName, int locationId);
        void Action_SetAllianceStatus(unsigned int playerId, unsigned int targetPlayerId, AllianceStatus status);
        void Action_LeaderboardControl(unsigned int stringId, unsigned int unitId);
        void Action_LeaderboardControlAtLocation(unsigned int stringId, unsigned int unitId, unsigned int locationId);
        void Action_LeaderboardResources(unsigned int stringId, CHK::ResourceType resourceType);
        void Action_LeaderboardKills(unsigned int stringId, unsigned int unitId);
        void Action_LeaderboardPoints(unsigned int stringId, CHK::ScoreType scoreType);
        void Action_LeaderboardGreed(unsigned int stringId);
        void Action_LeaderboardShowComputerPlayers(CHK::TriggerActionState state);

        const CHK::Trigger& GetTrigger()
        {
            return m_Trigger;
        }

        const unsigned int GetAddress() const
        {
            return m_Address;
        }

        bool HasChanges() const
        {
            return m_HasChanges;
        }

        private:
        bool m_HasChanges = false;
        unsigned int m_Address = 0;
        unsigned int m_NextCondition = 0;
        unsigned int m_NextAction = 0;
        CHK::Trigger m_Trigger;
        IIRInstruction* m_Instruction = nullptr;
        uint8_t m_PlayerMask = 7;
    };

}

#endif
