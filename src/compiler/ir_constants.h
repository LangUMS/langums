#ifndef __LANGUMS_IRCONSTANTS_H
#define __LANGUMS_IRCONSTANTS_H

namespace Langums
{

    enum ReservedRegisters
    {
        Reg_InstructionCounter = 0,
        Reg_CopyStorage,
        Reg_IndirectJumpAddress,
        Reg_Temp0,
        Reg_Temp1,
        Reg_Temp2,
        Reg_MulLeft,
        Reg_MulRight,
        Reg_ReservedEnd,

        Reg_StackTop = 0x574C
    };

    enum ReservedSwitches
    {
        Switch_InstructionCounterMutex,
        Switch_ArithmeticUnderflow,
        Switch_Random0,
        Switch_Random1,
        Switch_Random2,
        Switch_Random3,
        Switch_Random4,
        Switch_Random5,
        Switch_Random6,
        Switch_Random7,
        Switch_Player1,
        Switch_Player2,
        Switch_Player3,
        Switch_Player4,
        Switch_Player5,
        Switch_Player6,
        Switch_Player7,
        Switch_Player8,
        Switch_ReservedEnd
    };

    enum class ConditionComparison
    {
        AtLeast = 0,
        AtMost = 1,
        Exactly = 10
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

    enum class ModifyType
    {
        HitPoints,
        Energy,
        ShieldPoints,
        HangarCount
    };

    enum class EndGameType
    {
        Victory = 0,
        Defeat,
        Draw
    };

    enum class AllianceStatus
    {
        Enemy = 0,
        Ally,
        AlliedVictory
    };

    enum class LeaderboardType
    {
        ControlAtLocation = 0,
        Control,
        Greed,
        Kills,
        Points,
        Resources
    };

}

#endif
