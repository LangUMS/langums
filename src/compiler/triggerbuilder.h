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
        TriggerBuilder(int address, IIRInstruction* instruction, uint8_t playerMask);

        void CodeGen_TestReg(unsigned int regId, int value, CHK::TriggerComparisonType comparison);
        void CodeGen_TestSwitch(unsigned int switchId, bool expectedState);
        void CodeGen_Always();
        void CodeGen_Bring(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int locationId, unsigned int quantity);
        void CodeGen_Accumulate(unsigned int playerId, CHK::TriggerComparisonType comparison, CHK::ResourceType resourceType, unsigned int quantity);
        void CodeGen_ElapsedTime(CHK::TriggerComparisonType comparison, unsigned int quantity);
        void CodeGen_Commands(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity);
        void CodeGen_Kills(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity);
        void CodeGen_Deaths(unsigned int playerId, CHK::TriggerComparisonType comparison, unsigned int unitId, unsigned int quantity);
        void CodeGen_Countdown(CHK::TriggerComparisonType comparison, unsigned int time);

        void CodeGen_SetReg(unsigned int regId, int value);
        void CodeGen_IncReg(unsigned int regId, int amount);
        void CodeGen_DecReg(unsigned int regId, int amount);
        void CodeGen_DisplayMsg(unsigned int stringId);
        void CodeGen_JumpTo(unsigned int address);
        void CodeGen_PreserveTrigger();
        void CodeGen_Wait(unsigned int milliseconds);
        void CodeGen_Comment(unsigned int stringId);
        void CodeGen_SetSwitch(unsigned int switchId, CHK::TriggerActionState state);
        void CodeGen_CreateUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int locationId);
        void CodeGen_KillUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, int locationId);
        void CodeGen_MoveUnit(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int srcLocationId, unsigned int dstLocationId);
        void CodeGen_OrderUnit(unsigned int playerId, unsigned int unitId, CHK::TriggerActionState order, unsigned int srcLocationId, unsigned int dstLocationId);
        void CodeGen_ModifyUnitHitPoints(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId);
        void CodeGen_ModifyUnitEnergy(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId);
        void CodeGen_ModifyUnitShieldPoints(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId);
        void CodeGen_ModifyUnitHangarCount(unsigned int playerId, unsigned int unitId, unsigned int quantity, unsigned int amount, unsigned int locationId);
        void CodeGen_MoveLocation(unsigned int playerId, unsigned int unitId, unsigned int srcLocationId, unsigned int dstLocationId);
        void CodeGen_Victory();
        void CodeGen_Defeat();
        void CodeGen_Draw();
        void CodeGen_CenterView(unsigned int locationId);
        void CodeGen_Ping(unsigned int locationId);
        void CodeGen_SetResources(unsigned int playerId, unsigned int quantity, CHK::TriggerActionState action, CHK::ResourceType resourceType);
        void CodeGen_SetDeaths(unsigned int playerId, unsigned int unitId, unsigned int quantity, CHK::TriggerActionState action);
        void CodeGen_SetCountdown(unsigned int time, CHK::TriggerActionState action);

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
        uint8_t m_PlayerMask = 128;
    };

}

#endif
