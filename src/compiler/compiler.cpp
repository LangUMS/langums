#include <cctype>
#include <algorithm>

#include "../log.h"
#include "compiler.h"

namespace Langums
{

    Compiler::Compiler()
    {
        g_RegisterMap.clear();

        std::vector<uint8_t> unused;
        for (auto q = 0; q < sizeof(UnusedUnitTypes); q++)
        {
            unused.push_back((uint8_t)UnusedUnitTypes[q]);
        }

        std::sort(unused.begin(), unused.end());

        for (auto i = 0; i < 8; i++)
        {
            for (auto unitId : unused)
            {
                RegisterDef def;
                def.m_PlayerId = 7;
                def.m_Index = unitId;
                g_RegisterMap.push_back(def);
            }
        }
    }

    bool Compiler::Compile(const std::vector<std::unique_ptr<IIRInstruction>>& instructions, File& chk, bool preserveTriggers)
    {
        using namespace CHK;

        m_Triggers.clear();
        m_Triggers.reserve(MAX_TRIGGERS_COUNT);
        m_JmpPatchups.clear();

        m_File = &chk;
        m_StringsChunk = &chk.GetFirstChunk<CHKStringsChunk>(ChunkType::StringsChunk);
        if (m_StringsChunk == nullptr)
        {
            throw CompilerException("No strings chunk found in scenario file");
        }
         
        m_LocationsChunk = &chk.GetFirstChunk<CHKLocationsChunk>(ChunkType::LocationsChunk);
        if (m_LocationsChunk == nullptr)
        {
            throw CompilerException("No locations chunk found in scenario file");
        }

        m_CuwpChunk = &chk.GetFirstChunk<CHKCuwpChunk>(ChunkType::CuwpChunk);
        if (m_CuwpChunk == nullptr)
        {
            throw CompilerException("No CUWP slots chunk found in scenario file");
        }

        m_CuwpUsedChunk = &chk.GetFirstChunk<CHKCuwpUsedChunk>(ChunkType::CuwpUsedChunk);

        auto nextCuwpSlot = 0;

        m_StackPointer = g_RegisterMap.size() - 1;

        for (auto i = 0u; i < instructions.size(); i++) // emit event triggers first
        {
            auto& instruction = instructions[i];
            auto type = instruction->GetType();
            
            if (type == IRInstructionType::Unit)
            {
                auto unit = (IRUnitInstruction*)instruction.get();
                auto propsCount = unit->GetPropertyCount();
                if (propsCount == 0)
                {
                    throw CompilerException("Malformed input. Unit with zero properties");
                }

                auto slotIndex = nextCuwpSlot++;
                if (m_CuwpUsedChunk != nullptr)
                {
                    slotIndex = m_CuwpUsedChunk->FindFreeSlot();

                    if (slotIndex == -1)
                    {
                        throw CompilerException("All 64 unit slots are full");
                    }

                    m_CuwpUsedChunk->SetUsed(slotIndex, true);
                }

                auto slot = m_CuwpChunk->GetSlot(slotIndex);
                memset(slot, 0, sizeof(CUWPSlot));

                slot->m_OwnerId = 255;
                slot->m_ValidSpecialProperties = 24;

                i++;

                for (auto q = 0u; q < propsCount; q++)
                {
                    auto prop = (IRUnitPropInstruction*)instructions[i].get();
                    auto propType = prop->GetPropType();
                    auto value = prop->GetValue();

                    if (propType == UnitPropType::HitPoints)
                    {
                        slot->m_HitPoints = std::min(100u, value);
                        slot->m_ValidDataElements |= (uint16_t)CHK::CUWPDataElementFlag::HitPointsIsValid;
                    }
                    else if (propType == UnitPropType::ShieldPoints)
                    {
                        slot->m_ShieldPoints = std::min(100u, value);
                        slot->m_ValidDataElements |= (uint16_t)CHK::CUWPDataElementFlag::ShieldPointsIsValid;
                    }
                    else if (propType == UnitPropType::Energy)
                    {
                        slot->m_Energy = std::min(100u, value);
                        slot->m_ValidDataElements |= (uint16_t)CHK::CUWPDataElementFlag::EnergyIsValid;
                    }
                    else if (propType == UnitPropType::ResourceAmount)
                    {
                        slot->m_ResourceAmount = value;
                        slot->m_ValidDataElements |= (uint16_t)CHK::CUWPDataElementFlag::ResourceAmountIsValid;
                    }
                    else if (propType == UnitPropType::HangarCount)
                    {
                        slot->m_HangarCount = value;
                        slot->m_ValidDataElements |= (uint16_t)CHK::CUWPDataElementFlag::HangarCountIsValid;
                    }
                    else if (propType == UnitPropType::Cloaked)
                    {
                        if (value)
                        {
                            slot->m_Flags |= (uint16_t)CHK::CUWPFlag::Cloaked;
                            slot->m_ValidSpecialProperties |= (uint16_t)CHK::CUWPSpecialPropertiesFlag::CloakIsValid;
                        }
                    }
                    else if (propType == UnitPropType::Burrowed)
                    {
                        if (value)
                        {
                            slot->m_Flags |= (uint16_t)CHK::CUWPFlag::Burrowed;
                            slot->m_ValidSpecialProperties |= (uint16_t)CHK::CUWPSpecialPropertiesFlag::BurrowIsValid;
                        }
                    }
                    else if (propType == UnitPropType::InTransit)
                    {
                        if (value)
                        {
                            slot->m_Flags |= (uint16_t)CHK::CUWPFlag::InTransit;
                            slot->m_ValidSpecialProperties |= (uint16_t)CHK::CUWPSpecialPropertiesFlag::InTransitIsValid;
                        }
                    }
                    else if (propType == UnitPropType::Hallucinated)
                    {
                        if (value)
                        {
                            slot->m_Flags |= (uint16_t)CHK::CUWPFlag::Hallucinated;
                            slot->m_ValidSpecialProperties |= (uint16_t)CHK::CUWPSpecialPropertiesFlag::HallucinatedIsValid;
                        }
                    }
                    else if (propType == UnitPropType::Invincible)
                    {
                        if (value)
                        {
                            slot->m_Flags |= (uint16_t)CHK::CUWPFlag::Invincible;
                            slot->m_ValidSpecialProperties |= (uint16_t)CHK::CUWPSpecialPropertiesFlag::InvincibleIsValid;
                        }
                    }

                    i++;
                }
            }
            else if (type == IRInstructionType::Event)
            {
                auto evnt = (IREventInstruction*)instruction.get();
                auto conditionsCount = evnt->GetConditionsCount();
                if (conditionsCount == 0)
                {
                    throw CompilerException("Malformed input. Event with zero conditions");
                }

                TriggerBuilder eventTrigger(-1, nullptr, m_TriggersOwner);
                eventTrigger.Cond_TestSwitch(Switch_EventsMutex, false);

                auto switchId = evnt->GetSwitchId();
                eventTrigger.Action_SetSwitch(switchId, TriggerActionState::SetSwitch);

                for (auto q = 0u; q < conditionsCount; q++)
                {
                    i++;

                    auto& condition = instructions[i];
                    if (condition->GetType() == IRInstructionType::BringCond)
                    {
                        auto bring = (IRBringCondInstruction*)condition.get();
                        auto locationId = GetLocationIdByName(bring->GetLocationName());

                        eventTrigger.SetOwner(bring->GetPlayerId() + 1);

                        eventTrigger.Cond_Bring(
                            bring->GetPlayerId(),
                            (TriggerComparisonType)bring->GetComparison(),
                            bring->GetUnitId(),
                            locationId,
                            bring->GetQuantity()
                        );
                    }
                    else if (condition->GetType() == IRInstructionType::AccumCond)
                    {
                        auto accum = (IRAccumCondInstruction*)condition.get();

                        eventTrigger.SetOwner(accum->GetPlayerId() + 1);

                        eventTrigger.Cond_Accumulate(
                            accum->GetPlayerId(),
                            (TriggerComparisonType)accum->GetComparison(),
                            accum->GetResourceType(),
                            accum->GetQuantity()
                        );
                    }
                    else if (condition->GetType() == IRInstructionType::LeastResCond)
                    {
                        auto leastRes = (IRLeastResCondInstruction*)condition.get();
                        eventTrigger.SetOwner(leastRes->GetPlayerId() + 1);
                        eventTrigger.Cond_LeastResources(leastRes->GetPlayerId(), leastRes->GetResourceType());
                    }
                    else if (condition->GetType() == IRInstructionType::MostResCond)
                    {
                        auto mostRes = (IRMostResCondInstruction*)condition.get();
                        eventTrigger.SetOwner(mostRes->GetPlayerId() + 1);
                        eventTrigger.Cond_MostResources(mostRes->GetPlayerId(), mostRes->GetResourceType());
                    }
                    else if (condition->GetType() == IRInstructionType::ScoreCond)
                    {
                        auto score = (IRScoreCondInstruction*)condition.get();

                        eventTrigger.Cond_Score(
                            score->GetPlayerId(),
                            (TriggerComparisonType)score->GetComparison(),
                            score->GetScoreType(),
                            score->GetQuantity()
                        );
                    }
                    else if (condition->GetType() == IRInstructionType::LowScoreCond)
                    {
                        auto lowScore = (IRLowScoreCondInstruction*)condition.get();
                        eventTrigger.SetOwner(lowScore->GetPlayerId() + 1);
                        eventTrigger.Cond_LowestScore(lowScore->GetPlayerId(), lowScore->GetScoreType());
                    }
                    else if (condition->GetType() == IRInstructionType::HiScoreCond)
                    {
                        auto highScore = (IRHiScoreCondInstruction*)condition.get();
                        eventTrigger.SetOwner(highScore->GetPlayerId() + 1);
                        eventTrigger.Cond_LowestScore(highScore->GetPlayerId(), highScore->GetScoreType());
                    }
                    else if (condition->GetType() == IRInstructionType::TimeCond)
                    {
                        auto time = (IRTimeCondInstruction*)condition.get();

                        eventTrigger.Cond_ElapsedTime((TriggerComparisonType)time->GetComparison(), time->GetQuantity());
                    }
                    else if (condition->GetType() == IRInstructionType::CmdCond)
                    {
                        auto cmd = (IRCmdCondInstruction*)condition.get();
                        eventTrigger.SetOwner(cmd->GetPlayerId() + 1);
                        eventTrigger.Cond_Commands(cmd->GetPlayerId(), (TriggerComparisonType)cmd->GetComparison(), cmd->GetUnitId(), cmd->GetQuantity());
                    }
                    else if (condition->GetType() == IRInstructionType::CmdLeastCond)
                    {
                        auto cmdLeast = (IRCmdLeastCondInstruction*)condition.get();
                        eventTrigger.SetOwner(cmdLeast->GetPlayerId() + 1);

                        auto locationId = -1;
                        auto& locationName = cmdLeast->GetLocationName();

                        if (locationName.length() > 0)
                        {
                            locationId = GetLocationIdByName(locationName);
                        }

                        eventTrigger.Cond_CommandsLeast(cmdLeast->GetPlayerId(), cmdLeast->GetUnitId(), locationId);
                    }
                    else if (condition->GetType() == IRInstructionType::CmdMostCond)
                    {
                        auto cmdMost = (IRCmdMostCondInstruction*)condition.get();
                        eventTrigger.SetOwner(cmdMost->GetPlayerId() + 1);

                        auto locationId = -1;
                        auto& locationName = cmdMost->GetLocationName();

                        if (locationName.length() > 0)
                        {
                            locationId = GetLocationIdByName(locationName);
                        }

                        eventTrigger.Cond_CommandsMost(cmdMost->GetPlayerId(), cmdMost->GetUnitId(), locationId);
                    }
                    else if (condition->GetType() == IRInstructionType::KillCond)
                    {
                        auto kill = (IRKillCondInstruction*)condition.get();
                        eventTrigger.Cond_Kills(kill->GetPlayerId(), (TriggerComparisonType)kill->GetComparison(), kill->GetUnitId(), kill->GetQuantity());
                    }
                    else if (condition->GetType() == IRInstructionType::KillLeastCond)
                    {
                        auto killLeast = (IRKillLeastCondInstruction*)condition.get();
                        eventTrigger.SetOwner(killLeast->GetPlayerId() + 1);
                        eventTrigger.Cond_KillsLeast(killLeast->GetPlayerId(), killLeast->GetUnitId());
                    }
                    else if (condition->GetType() == IRInstructionType::KillMostCond)
                    {
                        auto killMost = (IRKillMostCondInstruction*)condition.get();
                        eventTrigger.SetOwner(killMost->GetPlayerId() + 1);
                        eventTrigger.Cond_KillsMost(killMost->GetPlayerId(), killMost->GetUnitId());
                    }
                    else if (condition->GetType() == IRInstructionType::DeathCond)
                    {
                        auto death = (IRDeathCondInstruction*)condition.get();
                        eventTrigger.Cond_Deaths(death->GetPlayerId(), (TriggerComparisonType)death->GetComparison(), death->GetUnitId(), death->GetQuantity());
                    }
                    else if (condition->GetType() == IRInstructionType::CountdownCond)
                    {
                        auto countdown = (IRCountdownCondInstruction*)condition.get();
                        eventTrigger.Cond_Countdown((TriggerComparisonType)countdown->GetComparison(), countdown->GetTime());
                    }
                    else if (condition->GetType() == IRInstructionType::OpponentsCond)
                    {
                        auto opponents = (IROpponentsCondInstruction*)condition.get();
                        eventTrigger.SetOwner(opponents->GetPlayerId() + 1);
                        eventTrigger.Cond_Opponents(opponents->GetPlayerId(), (TriggerComparisonType)opponents->GetComparison(), opponents->GetQuantity());
                    }
                }

                PushTriggers(eventTrigger.GetTriggers());
            }
            else
            {
                break;
            }
        }

        m_JumpTargets.clear();

        for (auto i = 0u; i < instructions.size(); i++)
        {
            auto& instruction = instructions[i];
            auto type = instruction->GetType();

            auto targetIndex = -1;

            if (type == IRInstructionType::Jmp)
            {
                auto jmp = (IRJmpInstruction*)instruction.get();
                targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
            }
            else if (type == IRInstructionType::JmpIfEq)
            {
                auto jmp = (IRJmpIfEqInstruction*)instruction.get();
                targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
            }
            else if (type == IRInstructionType::JmpIfNotEq)
            {
                auto jmp = (IRJmpIfNotEqInstruction*)instruction.get();
                targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
            }
            else if (type == IRInstructionType::JmpIfGrt)
            {
                auto jmp = (IRJmpIfGrtInstruction*)instruction.get();
                targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
            }
            else if (type == IRInstructionType::JmpIfLess)
            {
                auto jmp = (IRJmpIfLessInstruction*)instruction.get();
                targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
            }
            else if (type == IRInstructionType::JmpIfGrtOrEq)
            {
                auto jmp = (IRJmpIfGrtOrEqualInstruction*)instruction.get();
                targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
            }
            else if (type == IRInstructionType::JmpIfLessOrEq)
            {
                auto jmp = (IRJmpIfLessOrEqualInstruction*)instruction.get();
                targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
            }
            else if (type == IRInstructionType::JmpIfSwNotSet)
            {
                auto jmp = (IRJmpIfSwNotSetInstruction*)instruction.get();
                targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
            }
            else if (type == IRInstructionType::JmpIfSwSet)
            {
                auto jmp = (IRJmpIfSwSetInstruction*)instruction.get();
                targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
            }
            
            if (targetIndex >= 0)
            {
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto target = instructions[targetIndex].get();
                m_JumpTargets.insert(target);
            }
        }

        auto nextAddress = 0u;
        
        bool hasMulInstructions = false;
        for (auto& instruction : instructions)
        {
            if (instruction->GetType() == IRInstructionType::Mul)
            {
                hasMulInstructions = true;
                break;
            }
        }

        auto current = TriggerBuilder(nextAddress++, nullptr, m_TriggersOwner);

        bool needsIndirectJumps = false;

        if (hasMulInstructions)
        {
            EmitMulInstructionCode(nextAddress);
            needsIndirectJumps = true;
        }

        if (needsIndirectJumps)
        {
            EmitIndirectJumpCode(nextAddress);
        }

        for (auto i = 0u; i < instructions.size(); i++)
        {
            auto& instruction = instructions[i];

            if (m_JumpTargets.find(instruction.get()) != m_JumpTargets.end())
            {
                if (current.HasChanges())
                {
                    auto address = nextAddress++;
                    m_JumpAddresses[instruction.get()] = address;

                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());
                    current = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                }
                else
                {
                    m_JumpAddresses[instruction.get()] = current.GetAddress();
                }
            }

            if (instruction->GetType() == IRInstructionType::Push)
            {
                auto push = (IRPushInstruction*)instruction.get();
                if (push->IsValueLiteral())
                {
                    auto stackTop = m_StackPointer--;
                    current.Action_SetReg(stackTop, push->GetRegisterId());
                }
                else
                {
                    auto retAddress = nextAddress++;
                    
                    auto stackTop = m_StackPointer--;
                    auto copyAddress = CodeGen_CopyReg(stackTop, push->GetRegisterId(), nextAddress, retAddress);

                    // clear storage and jump to step 1
                    current.Action_SetReg(Reg_CopyStorage, 0);
                    current.Action_JumpTo(copyAddress);
                    PushTriggers(current.GetTriggers());
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Pop)
            {
                auto pop = (IRPopInstruction*)instruction.get();
                if (pop->GetRegisterId() == -1)
                {
                    ++m_StackPointer;
                }
                else
                {
                    auto regId = pop->GetRegisterId();

                    auto copyAddress = nextAddress++;
                    auto stackTop = ++m_StackPointer;

                    current.Action_SetReg(regId, 0);
                    current.Action_JumpTo(copyAddress);
                    PushTriggers(current.GetTriggers());

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto copy = TriggerBuilder(copyAddress, instruction.get(), m_TriggersOwner);
                        copy.Cond_TestReg(stackTop, i, TriggerComparisonType::AtLeast);
                        copy.Action_DecReg(stackTop, i);
                        copy.Action_IncReg(regId, i);
                        PushTriggers(copy.GetTriggers());
                    }

                    current = TriggerBuilder(copyAddress, instruction.get(), m_TriggersOwner);
                    current.Cond_TestReg(stackTop, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::SetReg)
            {
                auto setReg = (IRSetRegInstruction*)instruction.get();

                auto regId = setReg->GetRegisterId();
                if (regId >= Reg_StackTop)
                {
                    regId = m_StackPointer + (regId - Reg_StackTop) + 1;
                }

                current.Action_SetReg(regId, setReg->GetValue());
            }
            else if (instruction->GetType() == IRInstructionType::IncReg)
            {
                auto incReg = (IRIncRegInstruction*)instruction.get();

                auto regId = incReg->GetRegisterId();
                if (regId >= Reg_StackTop)
                {
                    regId = m_StackPointer + (regId - Reg_StackTop) + 1;
                }

                current.Action_IncReg(regId, incReg->GetAmount());
            }
            else if (instruction->GetType() == IRInstructionType::DecReg)
            {
                auto decReg = (IRDecRegInstruction*)instruction.get();
                auto regId = decReg->GetRegisterId();
                if (regId >= Reg_StackTop)
                {
                    regId = m_StackPointer + (regId - Reg_StackTop) + 1;
                }

                current.Action_DecReg(regId, decReg->GetAmount());
            }
            else if (instruction->GetType() == IRInstructionType::CopyReg)
            {
                auto retAddress = nextAddress++;

                auto copyReg = (IRCopyRegInstruction*)instruction.get();

                auto dstId = copyReg->GetDestinationRegisterId();
                if (dstId >= Reg_StackTop)
                {
                    dstId = m_StackPointer + (dstId - Reg_StackTop) + 1;
                }

                auto srcId = copyReg->GetSourceRegisterId();
                if (srcId >= Reg_StackTop)
                {
                    srcId = m_StackPointer + (srcId - Reg_StackTop) + 1;
                }

                auto copyAddress = CodeGen_CopyReg(dstId, srcId, nextAddress, retAddress);

                // clear storage and jump to step 1
                current.Action_SetReg(Reg_CopyStorage, 0);
                current.Action_JumpTo(copyAddress);

                PushTriggers(current.GetTriggers());
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
            }
            else if (instruction->GetType() == IRInstructionType::Add)
            {
                auto addAddress = nextAddress++;
                current.Action_JumpTo(addAddress);

                PushTriggers(current.GetTriggers());
                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                auto left = ++m_StackPointer;
                auto right = m_StackPointer + 1;

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto add = TriggerBuilder(addAddress, instruction.get(), m_TriggersOwner);
                    add.Cond_TestReg(left, i, TriggerComparisonType::AtLeast);
                    add.Action_DecReg(left, i);
                    add.Action_IncReg(right, i);
                    PushTriggers(add.GetTriggers());
                }

                auto finishAdd = TriggerBuilder(addAddress, instruction.get(), m_TriggersOwner);
                finishAdd.Cond_TestReg(left, 0, TriggerComparisonType::Exactly);
                finishAdd.Action_JumpTo(retAddress);
                PushTriggers(finishAdd.GetTriggers());
            }
            else if (instruction->GetType() == IRInstructionType::Sub)
            {
                auto subAddress = nextAddress++;
                current.Action_SetSwitch(Switch_ArithmeticUnderflow, TriggerActionState::ClearSwitch);
                current.Action_JumpTo(subAddress);

                PushTriggers(current.GetTriggers());
                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                auto left = ++m_StackPointer;
                auto right = m_StackPointer + 1;

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto sub = TriggerBuilder(subAddress, instruction.get(), m_TriggersOwner);
                    sub.Cond_TestReg(left, i, TriggerComparisonType::AtLeast);
                    sub.Cond_TestReg(right, i, TriggerComparisonType::AtLeast);
                    sub.Action_DecReg(left, i);
                    sub.Action_DecReg(right, i);
                    PushTriggers(sub.GetTriggers());
                }

                auto finishSub = TriggerBuilder(subAddress, instruction.get(), m_TriggersOwner);
                finishSub.Cond_TestReg(left, 0, TriggerComparisonType::Exactly);
                finishSub.Action_SetSwitch(Switch_ArithmeticUnderflow, TriggerActionState::ClearSwitch);
                finishSub.Action_JumpTo(retAddress);
                PushTriggers(finishSub.GetTriggers());

                finishSub = TriggerBuilder(subAddress, instruction.get(), m_TriggersOwner);
                finishSub.Cond_TestReg(left, 1, TriggerComparisonType::AtLeast);
                finishSub.Cond_TestReg(right, 0, TriggerComparisonType::Exactly);
                finishSub.Action_SetSwitch(Switch_ArithmeticUnderflow, TriggerActionState::SetSwitch);
                finishSub.Action_JumpTo(retAddress);
                PushTriggers(finishSub.GetTriggers());
            }
            else if (instruction->GetType() == IRInstructionType::Mul)
            {
                auto left = ++m_StackPointer;
                auto right = m_StackPointer + 1;

                auto mulAddress = ++nextAddress;
                auto mul2Address = ++nextAddress;
                auto mul3Address = ++nextAddress;

                current.Action_SetReg(Reg_MulLeft, 0);
                current.Action_SetReg(Reg_MulRight, 0);
                current.Action_JumpTo(mulAddress);
                PushTriggers(current.GetTriggers());

                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto moveLeft = TriggerBuilder(mulAddress, instruction.get(), m_TriggersOwner);
                    moveLeft.Cond_TestReg(left, i, TriggerComparisonType::AtLeast);
                    moveLeft.Action_DecReg(left, i);
                    moveLeft.Action_IncReg(Reg_MulLeft, i);
                    PushTriggers(moveLeft.GetTriggers());
                }

                auto moveLeftFinish = TriggerBuilder(mulAddress, instruction.get(), m_TriggersOwner);
                moveLeftFinish.Cond_TestReg(left, 0, TriggerComparisonType::Exactly);
                moveLeftFinish.Action_JumpTo(mul2Address);
                PushTriggers(moveLeftFinish.GetTriggers());

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto moveRight = TriggerBuilder(mul2Address, instruction.get(), m_TriggersOwner);
                    moveRight.Cond_TestReg(right, i, TriggerComparisonType::AtLeast);
                    moveRight.Action_DecReg(right, i);
                    moveRight.Action_IncReg(Reg_MulRight, i);
                    PushTriggers(moveRight.GetTriggers());
                }

                auto moveRightFinish = TriggerBuilder(mul2Address, instruction.get(), m_TriggersOwner);
                moveRightFinish.Cond_TestReg(right, 0, TriggerComparisonType::Exactly);
                moveRightFinish.Action_SetReg(Reg_IndirectJumpAddress, mul3Address);
                moveRightFinish.Action_JumpTo(m_MultiplyAddress);
                PushTriggers(moveRightFinish.GetTriggers());

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto push = TriggerBuilder(mul3Address, instruction.get(), m_TriggersOwner);
                    push.Cond_TestReg(Reg_MulRight, i, TriggerComparisonType::AtLeast);
                    push.Action_DecReg(Reg_MulRight, i);
                    push.Action_IncReg(right, i);
                    PushTriggers(push.GetTriggers());
                }

                auto pushDone = TriggerBuilder(mul3Address, instruction.get(), m_TriggersOwner);
                pushDone.Cond_TestReg(Reg_MulRight, 0, TriggerComparisonType::Exactly);
                pushDone.Action_JumpTo(retAddress);
                PushTriggers(pushDone.GetTriggers());
            }
            else if (instruction->GetType() == IRInstructionType::MulConst)
            {
                auto mulConst = (IRMulConstInstruction*)instruction.get();
                auto value = mulConst->GetValue();

                auto numShifts = 0;
                for (auto i = 1; i < 32; i++)
                {
                    numShifts += (value & (1 << i)) ? i : 0;
                }

                auto isOdd = value % 2;
                auto regId = m_StackPointer + 1;
                auto mulAddress = ++nextAddress;
                auto mulAddress2 = ++nextAddress;

                current.Action_SetReg(Reg_MulLeft, 0);
                current.Action_SetReg(Reg_MulRight, 0);
                current.Action_JumpTo(mulAddress);
                PushTriggers(current.GetTriggers());

                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto copy = TriggerBuilder(mulAddress, instruction.get(), m_TriggersOwner);
                    copy.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                    copy.Action_DecReg(regId, i);
                    copy.Action_IncReg(Reg_MulLeft, i);
                    copy.Action_IncReg(Reg_MulRight, i);
                    PushTriggers(copy.GetTriggers());
                }

                auto copyFinish = TriggerBuilder(mulAddress, instruction.get(), m_TriggersOwner);
                copyFinish.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                copyFinish.Action_JumpTo(mulAddress2);
                PushTriggers(copyFinish.GetTriggers());

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto mul = TriggerBuilder(mulAddress2, instruction.get(), m_TriggersOwner);
                    mul.Cond_TestReg(Reg_MulLeft, i, TriggerComparisonType::AtLeast);
                    mul.Action_DecReg(Reg_MulLeft, i);
                    mul.Action_IncReg(regId, i * (int)pow(2, numShifts));
                    PushTriggers(mul.GetTriggers());
                }

                auto mulFinish = TriggerBuilder(mulAddress2, instruction.get(), m_TriggersOwner);
                mulFinish.Cond_TestReg(Reg_MulLeft, 0, TriggerComparisonType::Exactly);
                
                auto mulAddress3 = -1;

                if (!isOdd)
                {
                    mulFinish.Action_JumpTo(retAddress);
                }
                else
                {
                    mulAddress3 = nextAddress++;
                    mulFinish.Action_JumpTo(mulAddress3);
                }

                PushTriggers(mulFinish.GetTriggers());

                if (isOdd)
                {
                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto addOdd = TriggerBuilder(mulAddress3, instruction.get(), m_TriggersOwner);
                        addOdd.Cond_TestReg(Reg_MulRight, i, TriggerComparisonType::AtLeast);
                        addOdd.Action_DecReg(Reg_MulRight, i);
                        addOdd.Action_IncReg(regId, i);
                        PushTriggers(addOdd.GetTriggers());
                    }

                    auto addOddFinish = TriggerBuilder(mulAddress3, instruction.get(), m_TriggersOwner);
                    addOddFinish.Cond_TestReg(Reg_MulRight, 0, TriggerComparisonType::Exactly);
                    addOddFinish.Action_JumpTo(retAddress);
                    PushTriggers(addOddFinish.GetTriggers());
                }
            }
            else if (instruction->GetType() == IRInstructionType::Div)
            {
                throw CompilerException("Malformed IR. Native Div is not implemented yet");
            }
            else if (instruction->GetType() == IRInstructionType::Rnd256)
            {
                auto rndAddress = nextAddress++;

                for (auto i = 0; i < 8; i++)
                {
                    current.Action_SetSwitch(Switch_Random0 + i, TriggerActionState::RandomizeSwitch);
                }

                auto stackTop = m_StackPointer--;
                current.Action_SetReg(stackTop, 0);
                current.Action_JumpTo(rndAddress);

                PushTriggers(current.GetTriggers());
                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                for (auto i = 0; i < 8; i++)
                {
                    auto rnd = TriggerBuilder(rndAddress, instruction.get(), m_TriggersOwner);
                    rnd.Cond_TestSwitch(Switch_Random0 + i, true);
                    rnd.Action_IncReg(stackTop, (1 << i));
                    PushTriggers(rnd.GetTriggers());
                }

                auto finish = TriggerBuilder(rndAddress, instruction.get(), m_TriggersOwner);
                finish.Action_JumpTo(retAddress);
                PushTriggers(finish.GetTriggers());
            }
            else if (instruction->GetType() == IRInstructionType::Jmp)
            {
                auto jmp = (IRJmpInstruction*)instruction.get();
                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();

                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto targetInstruction = instructions[targetIndex].get();

                PushTriggers(current.GetTriggers(), targetInstruction);
                auto trigger = &m_Triggers.back();

                current = TriggerBuilder(nextAddress++, instruction.get(), m_TriggersOwner);
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfEq)
            {
                auto jmp = (IRJmpIfEqInstruction*)instruction.get();
                auto value = jmp->GetValue();

                auto regId = jmp->GetRegisterId();

                if (regId > Reg_StackTop)
                {
                    throw CompilerException("Malformed IR. Testing past top of the stack");
                }

                if (regId == Reg_StackTop)
                {
                    regId = m_StackPointer + 1;
                }

                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto ifTrue = current;

                if (value == 0)
                {
                    current.Cond_TestReg(regId, 1, TriggerComparisonType::AtLeast);

                    ifTrue.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    PushTriggers(ifTrue.GetTriggers(), instructions[targetIndex].get());
                }
                else
                {
                    auto ifFalse = current;
                    current.Cond_TestReg(regId, value + 1, TriggerComparisonType::AtLeast);

                    ifFalse.Cond_TestReg(regId, (int)value - 1, TriggerComparisonType::AtMost);
                    current.AddSecondary(ifFalse);

                    ifTrue.Cond_TestReg(regId, value, TriggerComparisonType::Exactly);
                    PushTriggers(ifTrue.GetTriggers(), instructions[targetIndex].get());
                }
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfNotEq)
            {
                auto jmp = (IRJmpIfNotEqInstruction*)instruction.get();

                auto value = jmp->GetValue();
                auto regId = jmp->GetRegisterId();

                if (regId > Reg_StackTop)
                {
                    throw CompilerException("Malformed IR. Testing past top of the stack");
                }

                if (regId == Reg_StackTop)
                {
                    regId = m_StackPointer + 1;
                }

                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto ifTrue = current;

                if (value == 0)
                {
                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);

                    ifTrue.Cond_TestReg(regId, 1, TriggerComparisonType::AtLeast);
                    PushTriggers(ifTrue.GetTriggers(), instructions[targetIndex].get());
                }
                else
                {
                    auto ifFalse = current;
                    auto ifFalse2 = current;
                    ifFalse.Cond_TestReg(regId, value + 1, TriggerComparisonType::AtLeast);
                    PushTriggers(ifFalse.GetTriggers(), instructions[targetIndex].get());

                    ifFalse2.Cond_TestReg(regId, (int)value - 1, TriggerComparisonType::AtMost);
                    PushTriggers(ifFalse2.GetTriggers(), instructions[targetIndex].get());

                    current.Cond_TestReg(regId, value, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfGrt)
            {
                auto jmp = (IRJmpIfGrtInstruction*)instruction.get();
                auto value = jmp->GetValue();

                auto regId = jmp->GetRegisterId();

                if (regId > Reg_StackTop)
                {
                    throw CompilerException("Malformed IR. Testing past top of the stack");
                }

                if (regId == Reg_StackTop)
                {
                    regId = m_StackPointer + 1;
                }

                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto ifTrue = current;
                current.Cond_TestReg(regId, value, TriggerComparisonType::AtMost);

                ifTrue.Cond_TestReg(regId, value + 1, TriggerComparisonType::AtLeast);
                PushTriggers(ifTrue.GetTriggers(), instructions[targetIndex].get());
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfGrtOrEq)
            {
                auto jmp = (IRJmpIfGrtOrEqualInstruction*)instruction.get();
                auto value = jmp->GetValue();

                auto regId = jmp->GetRegisterId();

                if (regId > Reg_StackTop)
                {
                    throw CompilerException("Malformed IR. Testing past top of the stack");
                }

                if (regId == Reg_StackTop)
                {
                    regId = m_StackPointer + 1;
                }

                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto retAddress = nextAddress++;

                auto ifTrue = current;
                current.Cond_TestReg(regId, std::max(0, (int)value - 1), TriggerComparisonType::AtMost);

                ifTrue.Cond_TestReg(regId, value, TriggerComparisonType::AtLeast);
                PushTriggers(ifTrue.GetTriggers(), instructions[targetIndex].get());
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfLess)
            {
                auto jmp = (IRJmpIfLessInstruction*)instruction.get();
                auto value = jmp->GetValue();

                auto regId = jmp->GetRegisterId();

                if (regId > Reg_StackTop)
                {
                    throw CompilerException("Malformed IR. Testing past top of the stack");
                }

                if (regId == Reg_StackTop)
                {
                    regId = m_StackPointer + 1;
                }

                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                if (value > 0)
                {
                    auto ifTrue = current;
                    current.Cond_TestReg(regId, value, TriggerComparisonType::AtLeast);

                    ifTrue.Cond_TestReg(regId, (int)value - 1, TriggerComparisonType::AtMost);
                    PushTriggers(ifTrue.GetTriggers(), instructions[targetIndex].get());
                }
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfLessOrEq)
            {
                auto jmp = (IRJmpIfLessInstruction*)instruction.get();
                auto value = jmp->GetValue();

                auto regId = jmp->GetRegisterId();

                if (regId > Reg_StackTop)
                {
                    throw CompilerException("Malformed IR. Testing past top of the stack");
                }

                if (regId == Reg_StackTop)
                {
                    regId = m_StackPointer + 1;
                }

                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto ifTrue = current;
                current.Cond_TestReg(regId, value + 1, TriggerComparisonType::AtLeast);

                ifTrue.Cond_TestReg(regId, (int)value, TriggerComparisonType::AtMost);
                PushTriggers(ifTrue.GetTriggers(), instructions[targetIndex].get());
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfSwNotSet)
            {
                auto jmp = (IRJmpIfSwNotSetInstruction*)instruction.get();
                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto switchId = jmp->GetSwitchId();

                auto retAddress = nextAddress++;
                auto ifFalse = current;

                current.Cond_TestSwitch(switchId, true);

                ifFalse.Cond_TestSwitch(switchId, false);
                PushTriggers(ifFalse.GetTriggers(), instructions[targetIndex].get());
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfSwSet)
            {
                auto jmp = (IRJmpIfSwSetInstruction*)instruction.get();
                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto switchId = jmp->GetSwitchId();

                auto ifFalse = current;

                current.Cond_TestSwitch(switchId, false);

                ifFalse.Cond_TestSwitch(switchId, true);
                PushTriggers(ifFalse.GetTriggers(), instructions[targetIndex].get());
            }
            else if (instruction->GetType() == IRInstructionType::SetSw)
            {
                auto setSwitch = (IRSetSwInstruction*)instruction.get();
                current.Action_SetSwitch(setSwitch->GetSwitchId(), setSwitch->GetState() ? TriggerActionState::SetSwitch : TriggerActionState::ClearSwitch);
            }
            else if (instruction->GetType() == IRInstructionType::ChkPlayers)
            {
                auto startAddress = nextAddress++;

                for (auto i = 0u; i < 8; i++)
                {
                    current.Action_SetSwitch(Switch_Player1 + i, TriggerActionState::ClearSwitch);
                }

                current.Action_JumpTo(startAddress);
                PushTriggers(current.GetTriggers());
                current = TriggerBuilder(startAddress, nullptr, m_TriggersOwner);
                current.Action_SetSwitch(Switch_Player1 + m_TriggersOwner - 1, TriggerActionState::SetSwitch);
                current.Action_Wait(0);

                for (auto i = 0u; i < 8; i++)
                {
                    if (i + 1 == m_TriggersOwner)
                    {
                        continue;
                    }

                    auto checkIfPlayerActive = TriggerBuilder(startAddress, nullptr, i + 1);
                    checkIfPlayerActive.Action_SetSwitch(Switch_Player1 + i, TriggerActionState::SetSwitch);
                    PushTriggers(checkIfPlayerActive.GetTriggers());
                }
            }
            else if (instruction->GetType() == IRInstructionType::IsPresent)
            {
                auto isPresent = (IRIsPresentInstruction*)instruction.get();
                auto& playerIds = isPresent->GetPlayerIds();

                auto address = nextAddress++;
                auto retAddress = nextAddress++;

                auto stackTop = m_StackPointer--;

                current.Action_JumpTo(address);
                current.Action_SetReg(stackTop, 0);
                PushTriggers(current.GetTriggers());

                auto checkPresent = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                checkPresent.Action_Wait(0);
                checkPresent.Action_JumpTo(retAddress);
                PushTriggers(checkPresent.GetTriggers());

                for (auto& playerId : playerIds)
                {
                    auto countPresent = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    countPresent.Cond_TestSwitch(Switch_Player1 + playerId, true);
                    countPresent.Action_IncReg(stackTop, 1);
                    PushTriggers(countPresent.GetTriggers());
                }

                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
            }
            else if (instruction->GetType() == IRInstructionType::DisplayMsg)
            {
                auto displayMsgReg = (IRDisplayMsgInstruction*)instruction.get();
                auto playerId = displayMsgReg->GetPlayerId();
                auto stringId = m_StringsChunk->InsertString(displayMsgReg->GetMessage());

                if (playerId + 1 == m_TriggersOwner)
                {
                    current.Action_DisplayMsg(stringId);
                }
                else
                {
                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    PushTriggers(waitTrigger.GetTriggers());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner - 1;
                    }

                    auto msgTrigger = TriggerBuilder(address, instruction.get(), playerId + 1);
                    msgTrigger.Action_DisplayMsg(stringId);

                    if (all)
                    {
                        msgTrigger.SetExecuteForAllPlayers();
                    }

                    PushTriggers(msgTrigger.GetTriggers());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Wait)
            {
                auto wait = (IRWaitInstruction*)instruction.get();
                current.Action_Wait(wait->GetMilliseconds());
            }
            else if (instruction->GetType() == IRInstructionType::Spawn)
            {
                auto spawn = (IRSpawnInstruction*)instruction.get();
                auto unitSlot = spawn->GetPropsSlot();

                auto locationId = GetLocationIdByName(spawn->GetLocationName());
                
                if (spawn->IsValueLiteral())
                {
                    current.Action_CreateUnit(spawn->GetPlayerId(), spawn->GetUnitId(), spawn->GetRegisterId(), locationId, unitSlot);
                }
                else
                {
                    auto regId = spawn->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! Spawn expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto spawnTrigger = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        spawnTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        spawnTrigger.Action_DecReg(regId, i);
                        spawnTrigger.Action_CreateUnit(spawn->GetPlayerId(), spawn->GetUnitId(), i, locationId, unitSlot);
                        PushTriggers(spawnTrigger.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Kill)
            {
                auto kill = (IRKillInstruction*)instruction.get();

                auto locationId = -1;
                auto& locName = kill->GetLocationName();
                if (locName.length() != 0)
                {
                    locationId = GetLocationIdByName(locName);
                }

                if (kill->IsValueLiteral())
                {
                    current.Action_KillUnit(kill->GetPlayerId(), kill->GetUnitId(), kill->GetRegisterId(), locationId);
                }
                else
                {
                    auto regId = kill->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! Kill expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto killTrigger = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        killTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        killTrigger.Action_DecReg(regId, i);
                        killTrigger.Action_KillUnit(kill->GetPlayerId(), kill->GetUnitId(), i, locationId);
                        PushTriggers(killTrigger.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Remove)
            {
                auto remove = (IRRemoveInstruction*)instruction.get();

                auto locationId = -1;
                auto locName = remove->GetLocationName();
                if (locName.length() != 0)
                {
                    locationId = GetLocationIdByName(locName);
                }

                if (remove->IsValueLiteral())
                {
                    current.Action_RemoveUnit(remove->GetPlayerId(), remove->GetUnitId(), remove->GetRegisterId(), locationId);
                }
                else
                {
                    auto regId = remove->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! Remove expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto removeTrigger = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        removeTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        removeTrigger.Action_DecReg(regId, i);
                        removeTrigger.Action_RemoveUnit(remove->GetPlayerId(), remove->GetUnitId(), i, locationId);
                        PushTriggers(removeTrigger.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Move)
            {
                auto move = (IRMoveInstruction*)instruction.get();

                auto srcLocationId = GetLocationIdByName(move->GetSrcLocationName());
                auto dstLocationId = GetLocationIdByName(move->GetDstLocationName());

                if (move->IsValueLiteral())
                {
                    current.Action_MoveUnit(move->GetPlayerId(), move->GetUnitId(), move->GetRegisterId(), srcLocationId, dstLocationId);
                }
                else
                {
                    auto regId = move->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! Move expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto moveTrigger = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        moveTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        moveTrigger.Action_DecReg(regId, i);
                        current.Action_MoveUnit(move->GetPlayerId(), move->GetUnitId(), i, srcLocationId, dstLocationId);
                        PushTriggers(moveTrigger.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Order)
            {
                auto order = (IROrderInstruction*)instruction.get();

                auto srcLocationId = GetLocationIdByName(order->GetSrcLocationName());
                auto dstLocationId = GetLocationIdByName(order->GetDstLocationName());

                current.Action_OrderUnit(order->GetPlayerId(), order->GetUnitId(), order->GetOrder(), srcLocationId, dstLocationId);
            }
            else if (instruction->GetType() == IRInstructionType::Modify)
            {
                auto modify = (IRModifyInstruction*)instruction.get();

                auto locationId = GetLocationIdByName(modify->GetLocationName());
                auto playerId = modify->GetPlayerId();
                auto unitId = modify->GetUnitId();
                auto quantity = modify->GetRegisterId();
                auto amount = modify->GetAmount();

                if (modify->IsValueLiteral())
                {
                    switch (modify->GetModifyType())
                    {
                    case ModifyType::HitPoints:
                        current.Action_ModifyUnitHP(playerId, unitId, quantity, amount, locationId);
                        break;
                    case ModifyType::Energy:
                        current.Action_ModifyUnitEnergy(playerId, unitId, quantity, amount, locationId);
                        break;
                    case ModifyType::ShieldPoints:
                        current.Action_ModifyUnitSP(playerId, unitId, quantity, amount, locationId);
                        break;
                    case ModifyType::HangarCount:
                        current.Action_ModifyUnitHangar(playerId, unitId, quantity, amount, locationId);
                        break;
                    }
                }
                else
                {
                    auto regId = modify->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! Modify expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto modifyTrigger = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        modifyTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        modifyTrigger.Action_DecReg(regId, i);

                        switch (modify->GetModifyType())
                        {
                        case ModifyType::HitPoints:
                            current.Action_ModifyUnitHP(playerId, unitId, i, amount, locationId);
                            break;
                        case ModifyType::Energy:
                            current.Action_ModifyUnitEnergy(playerId, unitId, i, amount, locationId);
                            break;
                        case ModifyType::ShieldPoints:
                            current.Action_ModifyUnitSP(playerId, unitId, i, amount, locationId);
                            break;
                        case ModifyType::HangarCount:
                            current.Action_ModifyUnitHangar(playerId, unitId, i, amount, locationId);
                            break;
                        }

                        PushTriggers(modifyTrigger.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Give)
            {
                auto give = (IRGiveInstruction*)instruction.get();

                auto locationId = GetLocationIdByName(give->GetLocationName());
                auto srcPlayerId = give->GetSrcPlayerId();
                auto dstPlayerId = give->GetDstPlayerId();
                auto unitId = give->GetUnitId();

                if (give->IsValueLiteral())
                {
                    auto quantity = give->GetRegisterId();
                    current.Action_GiveUnits(srcPlayerId, dstPlayerId, unitId, quantity, locationId);
                }
                else
                {
                    auto regId = give->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! Give expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto giveTrigger = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        giveTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        giveTrigger.Action_DecReg(regId, i);
                        giveTrigger.Action_GiveUnits(srcPlayerId, dstPlayerId, unitId, i, locationId);
                        PushTriggers(giveTrigger.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::MoveLoc)
            {
                auto move = (IRMoveLocInstruction*)instruction.get();

                auto srcLocationId = GetLocationIdByName(move->GetSrcLocationName());
                auto dstLocationId = GetLocationIdByName(move->GetDstLocationName());
                
                current.Action_MoveLocation(move->GetPlayerId(), move->GetUnitId(), srcLocationId, dstLocationId);
            }
            else if (instruction->GetType() == IRInstructionType::EndGame)
            {
                auto endGame = (IREndGameInstruction*)instruction.get();

                auto type = endGame->GetEndGameType();
                auto playerId = endGame->GetPlayerId();

                if (playerId + 1 == m_TriggersOwner)
                {
                    if (type == EndGameType::Victory)
                    {
                        current.Action_Victory();
                    }
                    else if (type == EndGameType::Defeat)
                    {
                        current.Action_Defeat();
                    }
                    else if (type == EndGameType::Draw)
                    {
                        current.Action_Draw();
                    }
                }
                else
                {
                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    PushTriggers(waitTrigger.GetTriggers());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner - 1;
                    }

                    auto endGameTrigger = TriggerBuilder(address, instruction.get(), playerId + 1);

                    if (type == EndGameType::Victory)
                    {
                        endGameTrigger.Action_Victory();
                    }
                    else if (type == EndGameType::Defeat)
                    {
                        endGameTrigger.Action_Defeat();
                    }
                    else if (type == EndGameType::Draw)
                    {
                        endGameTrigger.Action_Draw();
                    }

                    if (all)
                    {
                        endGameTrigger.SetExecuteForAllPlayers();
                    }

                    PushTriggers(endGameTrigger.GetTriggers());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::CenterView)
            {
                auto centerView = (IRCenterViewInstruction*)instruction.get();

                auto locationId = GetLocationIdByName(centerView->GetLocationName());
                auto playerId = centerView->GetPlayerId();

                if (playerId + 1 == m_TriggersOwner)
                {
                    current.Action_CenterView(locationId + 1);
                }
                else
                {
                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    PushTriggers(waitTrigger.GetTriggers());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner - 1;
                    }

                    auto centerTrigger = TriggerBuilder(address, instruction.get(), playerId + 1);
                    centerTrigger.Action_CenterView(locationId + 1);

                    if (all)
                    {
                        centerTrigger.SetExecuteForAllPlayers();
                    }

                    PushTriggers(centerTrigger.GetTriggers());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Ping)
            {
                auto ping = (IRPingInstruction*)instruction.get();

                auto locationId = GetLocationIdByName(ping->GetLocationName());
                auto playerId = ping->GetPlayerId();

                if (playerId + 1 == m_TriggersOwner)
                {
                    current.Action_Ping(locationId + 1);
                }
                else
                {
                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    PushTriggers(waitTrigger.GetTriggers());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner - 1;
                    }

                    auto pingTrigger = TriggerBuilder(address, instruction.get(), playerId + 1);
                    pingTrigger.Action_Ping(locationId + 1);

                    if (all)
                    {
                        pingTrigger.SetExecuteForAllPlayers();
                    }

                    PushTriggers(pingTrigger.GetTriggers());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::SetResource)
            {
                auto setResource = (IRSetResourceInstruction*)instruction.get();
                auto playerId = setResource->GetPlayerId();

                if (setResource->IsValueLiteral())
                {
                    auto quantity = setResource->GetRegisterId();
                    current.Action_SetResources(playerId, quantity, TriggerActionState::SetTo, setResource->GetResourceType());
                }
                else
                {
                    auto regId = setResource->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! SetResource expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    auto address = nextAddress++;
                    current.Action_SetResources(playerId, 0, TriggerActionState::SetTo, setResource->GetResourceType());
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        add.Action_SetResources(playerId, i, TriggerActionState::Add, setResource->GetResourceType());
                        PushTriggers(add.GetTriggers());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    PushTriggers(finishAdd.GetTriggers());
                }
            }
            else if (instruction->GetType() == IRInstructionType::IncResource)
            {
                auto setResource = (IRIncResourceInstruction*)instruction.get();
                auto playerId = setResource->GetPlayerId();

                if (setResource->IsValueLiteral())
                {
                    auto quantity = setResource->GetRegisterId();
                    current.Action_SetResources(playerId, quantity, TriggerActionState::Add, setResource->GetResourceType());
                }
                else
                {
                    auto regId = setResource->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! IncResource expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        add.Action_SetResources(playerId, i, TriggerActionState::Add, setResource->GetResourceType());
                        PushTriggers(add.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::DecResource)
            {
                auto setResource = (IRDecResourceInstruction*)instruction.get();
                auto playerId = setResource->GetPlayerId();

                if (setResource->IsValueLiteral())
                {
                    auto quantity = setResource->GetRegisterId();
                    current.Action_SetResources(playerId, quantity, TriggerActionState::Subtract, setResource->GetResourceType());
                }
                else
                {
                    auto regId = setResource->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! DecResource expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto sub = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        sub.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        sub.Action_DecReg(regId, i);
                        sub.Action_SetResources(playerId, i, TriggerActionState::Subtract, setResource->GetResourceType());
                        PushTriggers(sub.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::SetScore)
            {
                auto setScore = (IRSetScoreInstruction*)instruction.get();
                auto playerId = setScore->GetPlayerId();

                if (setScore->IsValueLiteral())
                {
                    auto quantity = setScore->GetRegisterId();
                    current.Action_SetScore(playerId, quantity, TriggerActionState::SetTo, setScore->GetScoreType());
                }
                else
                {
                    auto regId = setScore->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! SetScore expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    auto address = nextAddress++;
                    current.Action_SetScore(playerId, 0, TriggerActionState::SetTo, setScore->GetScoreType());
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetScore(playerId, i, TriggerActionState::Add, setScore->GetScoreType());
                        PushTriggers(add.GetTriggers());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    PushTriggers(finishAdd.GetTriggers());
                }
            }
            else if (instruction->GetType() == IRInstructionType::IncScore)
            {
                auto incScore = (IRIncScoreInstruction*)instruction.get();
                auto playerId = incScore->GetPlayerId();

                if (incScore->IsValueLiteral())
                {
                    auto quantity = incScore->GetRegisterId();
                    current.Action_SetScore(playerId, quantity, TriggerActionState::Add, incScore->GetScoreType());
                }
                else
                {
                    auto regId = incScore->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! IncScore expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetScore(playerId, i, TriggerActionState::Add, incScore->GetScoreType());
                        PushTriggers(add.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::DecScore)
            {
                auto incScore = (IRIncScoreInstruction*)instruction.get();
                auto playerId = incScore->GetPlayerId();

                if (incScore->IsValueLiteral())
                {
                    auto quantity = incScore->GetRegisterId();
                    current.Action_SetScore(playerId, quantity, TriggerActionState::Subtract, incScore->GetScoreType());
                }
                else
                {
                    auto regId = incScore->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! DecScore expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetScore(playerId, i, TriggerActionState::Subtract, incScore->GetScoreType());
                        PushTriggers(add.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::SetCountdown)
            {
                auto setCountdown = (IRSetCountdownInstruction*)instruction.get();

                if (setCountdown->IsValueLiteral())
                {
                    auto time = setCountdown->GetRegisterId();
                    current.Action_SetCountdown(time, TriggerActionState::SetTo);
                }
                else
                {
                    auto regId = setCountdown->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! SetCountdown expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    auto address = nextAddress++;
                    current.Action_SetCountdown(0, TriggerActionState::SetTo);
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        add.Action_SetCountdown(i, TriggerActionState::Add);
                        PushTriggers(add.GetTriggers());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    PushTriggers(finishAdd.GetTriggers());
                }
            }
            else if (instruction->GetType() == IRInstructionType::AddCountdown)
            {
                auto addCountdown = (IRAddCountdownInstruction*)instruction.get();

                if (addCountdown->IsValueLiteral())
                {
                    auto time = addCountdown->GetRegisterId();
                    current.Action_SetCountdown(time, TriggerActionState::Add);
                }
                else
                {
                    auto regId = addCountdown->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! AddCountdown expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        add.Action_SetCountdown(i, TriggerActionState::Add);
                        PushTriggers(add.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::SubCountdown)
            {
                auto subCountdown = (IRSubCountdownInstruction*)instruction.get();

                if (subCountdown->IsValueLiteral())
                {
                    auto time = subCountdown->GetRegisterId();
                    current.Action_SetCountdown(time, TriggerActionState::Subtract);
                }
                else
                {
                    auto regId = subCountdown->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! SubCountdown expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        add.Action_SetCountdown(i, TriggerActionState::Subtract);
                        PushTriggers(add.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::PauseCountdown)
            {
                auto pauseCountdown = (IRPauseCountdownInstruction*)instruction.get();

                if (pauseCountdown->IsUnpause())
                {
                    current.Action_UnpauseCountdown();
                }
                else
                {
                    current.Action_PauseCountdown();
                }
            }
            else if (instruction->GetType() == IRInstructionType::MuteUnitSpeech)
            {
                auto muteUnit = (IRMuteUnitSpeechInstruction*)instruction.get();

                if (muteUnit->IsUnmute())
                {
                    current.Action_UnmuteUnitSpeech();
                }
                else
                {
                    current.Action_MuteUnitSpeech();
                }
            }
            else if (instruction->GetType() == IRInstructionType::SetDeaths)
            {
                auto setDeaths = (IRSetDeathsInstruction*)instruction.get();
                auto playerId = setDeaths->GetPlayerId();

                if (setDeaths->IsValueLiteral())
                {
                    auto quantity = setDeaths->GetRegisterId();
                    current.Action_SetDeaths(playerId, setDeaths->GetUnitId(), quantity, TriggerActionState::SetTo);
                }
                else
                {
                    auto regId = setDeaths->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! SetDeaths expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    auto address = nextAddress++;
                    current.Action_SetDeaths(playerId, setDeaths->GetUnitId(), 0, TriggerActionState::SetTo);
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetDeaths(playerId, setDeaths->GetUnitId(), i, TriggerActionState::Add);
                        PushTriggers(add.GetTriggers());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    PushTriggers(finishAdd.GetTriggers());
                }
            }
            else if (instruction->GetType() == IRInstructionType::IncDeaths)
            {
                auto incDeaths = (IRIncDeathsInstruction*)instruction.get();
                auto playerId = incDeaths->GetPlayerId();

                if (incDeaths->IsValueLiteral())
                {
                    auto quantity = incDeaths->GetRegisterId();
                    current.Action_SetDeaths(playerId, incDeaths->GetUnitId(), quantity, TriggerActionState::Add);
                }
                else
                {
                    auto regId = incDeaths->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! IncDeaths expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetDeaths(playerId, incDeaths->GetUnitId(), i, TriggerActionState::Add);
                        PushTriggers(add.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::DecDeaths)
            {
                auto decDeaths = (IRDecDeathsInstruction*)instruction.get();
                auto playerId = decDeaths->GetPlayerId();

                if (decDeaths->IsValueLiteral())
                {
                    auto quantity = decDeaths->GetRegisterId();
                    current.Action_SetDeaths(playerId, decDeaths->GetUnitId(), quantity, TriggerActionState::Subtract);
                }
                else
                {
                    auto regId = decDeaths->GetRegisterId();
                    if (regId != Reg_StackTop)
                    {
                        throw CompilerException(SafePrintf("Malformed IR! DecDeaths expects the quantity on top of the stack."));
                    }

                    regId = ++m_StackPointer;

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(current.GetAddress(), instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetDeaths(playerId, decDeaths->GetUnitId(), i, TriggerActionState::Subtract);
                        PushTriggers(add.GetTriggers());
                    }

                    current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Talk)
            {
                auto talk = (IRTalkInstruction*)instruction.get();

                auto playerId = talk->GetPlayerId();

                if (playerId + 1 == m_TriggersOwner)
                {
                    current.Action_TalkingPortrait(talk->GetUnitId(), talk->GetTime());
                }
                else
                {
                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    PushTriggers(waitTrigger.GetTriggers());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner - 1;
                    }

                    auto talkTrigger = TriggerBuilder(address, instruction.get(), playerId + 1);
                    talkTrigger.Action_TalkingPortrait(talk->GetUnitId(), talk->GetTime());

                    if (all)
                    {
                        talkTrigger.SetExecuteForAllPlayers();
                    }

                    PushTriggers(talkTrigger.GetTriggers());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::SetDoodad)
            {
                auto setDoodad = (IRSetDoodadInstruction*)instruction.get();
                auto locationId = GetLocationIdByName(setDoodad->GetLocationName());

                current.Action_SetDoodadState(setDoodad->GetPlayerId(), setDoodad->GetUnitId(), setDoodad->GetState(), locationId);
            }
            else if (instruction->GetType() == IRInstructionType::SetInvincible)
            {
                auto setInvincible = (IRSetInvincibleInstruction*)instruction.get();
                auto locationId = GetLocationIdByName(setInvincible->GetLocationName());

                current.Action_SetInvincibility(setInvincible->GetPlayerId(), setInvincible->GetUnitId(), setInvincible->GetState(), locationId);
            }
            else if (instruction->GetType() == IRInstructionType::AIScript)
            {
                auto aiScript = (IRAIScriptInstruction*)instruction.get();

                auto playerId = aiScript->GetPlayerId();

                auto locationId = -1;
                auto& locName = aiScript->GetLocationName();
                if (locName.length() > 0)
                {
                    locationId = GetLocationIdByName(locName);
                }

                if (playerId + 1 == m_TriggersOwner)
                {
                    current.Action_RunAIScript(playerId, aiScript->GetScriptName(), locationId);
                }
                else
                {
                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    PushTriggers(waitTrigger.GetTriggers());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner - 1;
                    }

                    auto aiTrigger = TriggerBuilder(address, instruction.get(), playerId + 1);
                    aiTrigger.Action_RunAIScript(aiScript->GetPlayerId(), aiScript->GetScriptName(), locationId);

                    if (all)
                    {
                        aiTrigger.SetExecuteForAllPlayers();
                    }

                    PushTriggers(aiTrigger.GetTriggers());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::SetAlly)
            {
                auto setAlly = (IRSetAllyInstruction*)instruction.get();

                auto playerId = setAlly->GetPlayerId();
                auto targetPlayerId = setAlly->GetTargetPlayerId();

                if (playerId + 1 == m_TriggersOwner)
                {
                    current.Action_SetAllianceStatus(playerId, targetPlayerId, setAlly->GetAllianceStatus());
                }
                else
                {
                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    PushTriggers(waitTrigger.GetTriggers());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner - 1;
                    }

                    auto allyTrigger = TriggerBuilder(address, instruction.get(), playerId + 1);
                    allyTrigger.Action_SetAllianceStatus(setAlly->GetPlayerId(), targetPlayerId, setAlly->GetAllianceStatus());

                    if (all)
                    {
                        allyTrigger.SetExecuteForAllPlayers();
                    }

                    PushTriggers(allyTrigger.GetTriggers());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::SetObj)
            {
                auto setObj = (IRSetObjInstruction*)instruction.get();
                auto stringId = m_StringsChunk->InsertString(setObj->GetText());
                auto playerId = setObj->GetPlayerId();

                if (playerId + 1 == m_TriggersOwner)
                {
                    current.Action_SetMissionObjectives(stringId);
                }
                else
                {
                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    PushTriggers(waitTrigger.GetTriggers());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner - 1;
                    }

                    auto setObjTrigger = TriggerBuilder(address, instruction.get(), playerId + 1);
                    setObjTrigger.Action_SetMissionObjectives(stringId);

                    if (all)
                    {
                        setObjTrigger.SetExecuteForAllPlayers();
                    }

                    PushTriggers(setObjTrigger.GetTriggers());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::PauseGame)
            {
                auto pause = (IRPauseGameInstruction*)instruction.get();
                if (pause->IsUnpause())
                {
                    current.Action_UnpauseGame();
                }
                else
                {
                    current.Action_PauseGame();
                }
            }
            else if (instruction->GetType() == IRInstructionType::NextScen)
            {
                auto nextScenario = (IRNextScenInstruction*)instruction.get();
                auto& name = nextScenario->GetName();
                auto stringId = m_StringsChunk->InsertString(name);
                current.Action_SetNextScenario(stringId);
            }
            else if (instruction->GetType() == IRInstructionType::Leaderboard)
            {
                auto leaderboard = (IRLeaderboardInstruction*)instruction.get();
                auto stringId = m_StringsChunk->InsertString(leaderboard->GetText());

                auto type = leaderboard->GetLeaderboardType();
                if (type == LeaderboardType::Control)
                {
                    current.Action_LeaderboardControl(stringId, leaderboard->GetUnitId());
                }
                else if (type == LeaderboardType::ControlAtLocation)
                {
                    auto locationId = GetLocationIdByName(leaderboard->GetLocationName());
                    current.Action_LeaderboardControlAtLocation(stringId, leaderboard->GetUnitId(), locationId);
                }
                else if (type == LeaderboardType::Greed)
                {
                    current.Action_LeaderboardGreed(stringId);
                }
                else if (type == LeaderboardType::Kills)
                {
                    current.Action_LeaderboardKills(stringId, leaderboard->GetUnitId());
                }
                else if (type == LeaderboardType::Points)
                {
                    current.Action_LeaderboardPoints(stringId, leaderboard->GetScoreType());
                }
                else if (type == LeaderboardType::Resources)
                {
                    current.Action_LeaderboardResources(stringId, leaderboard->GetResourceType());
                }
            }
            else if (instruction->GetType() == IRInstructionType::LeaderboardCpu)
            {
                auto leaderboardCpu = (IRLeaderboardCpuInstruction*)instruction.get();
                current.Action_LeaderboardShowComputerPlayers(leaderboardCpu->GetState());
            }
            else if (instruction->GetType() == IRInstructionType::PlayWAV)
            {
                auto playWav = (IRPlayWAVInstruction*)instruction.get();
                auto name = SafePrintf("staredit\\wav\\%", playWav->GetWavName());
                auto wavStringId = m_StringsChunk->InsertString(name);
                auto wavTime = playWav->GetWavTime();
                auto playerId = playWav->GetPlayerId();

                if (playerId + 1 == m_TriggersOwner)
                {
                    current.Action_PlayWAV(wavStringId, wavTime);
                }
                else
                {
                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    PushTriggers(current.GetTriggers());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    PushTriggers(waitTrigger.GetTriggers());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner - 1;
                    }

                    auto wavTrigger = TriggerBuilder(address, instruction.get(), playerId + 1);
                    wavTrigger.Action_PlayWAV(wavStringId, wavTime);

                    if (all)
                    {
                        wavTrigger.SetExecuteForAllPlayers();
                    }

                    PushTriggers(wavTrigger.GetTriggers());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Transmission)
            {
                auto transmission = (IRTransmissionInstructrion*)instruction.get();

                auto stringId = m_StringsChunk->InsertString(transmission->GetLocationName());
                auto name = SafePrintf("staredit\\wav\\%", transmission->GetWavName());
                auto wavStringId = m_StringsChunk->InsertString(name);
                auto locationId = GetLocationIdByName(transmission->GetLocationName());

                auto time = transmission->GetTime();
                current.Action_Transmission(stringId, transmission->GetUnitId(), locationId, time, CHK::TriggerActionState::SetTo, wavStringId, transmission->GetWavTime());
            }
            else if
            (
                instruction->GetType() == IRInstructionType::Nop ||
                instruction->GetType() == IRInstructionType::Unit ||
                instruction->GetType() == IRInstructionType::UnitProp ||
                instruction->GetType() == IRInstructionType::Event ||
                instruction->GetType() == IRInstructionType::BringCond ||
                instruction->GetType() == IRInstructionType::AccumCond ||
                instruction->GetType() == IRInstructionType::LeastResCond ||
                instruction->GetType() == IRInstructionType::MostResCond ||
                instruction->GetType() == IRInstructionType::ScoreCond ||
                instruction->GetType() == IRInstructionType::HiScoreCond ||
                instruction->GetType() == IRInstructionType::LowScoreCond ||
                instruction->GetType() == IRInstructionType::TimeCond ||
                instruction->GetType() == IRInstructionType::KillCond ||
                instruction->GetType() == IRInstructionType::KillMostCond ||
                instruction->GetType() == IRInstructionType::KillLeastCond ||
                instruction->GetType() == IRInstructionType::DeathCond ||
                instruction->GetType() == IRInstructionType::CmdCond ||
                instruction->GetType() == IRInstructionType::CmdLeastCond ||
                instruction->GetType() == IRInstructionType::CmdMostCond ||
                instruction->GetType() == IRInstructionType::CountdownCond ||
                instruction->GetType() == IRInstructionType::OpponentsCond
                )
            {
                continue;
            }
            else
            {
                throw CompilerException("Unknown instruction type");
            }
        }

        PushTriggers(current.GetTriggers());

        for (auto& patchup : m_JmpPatchups)
        {
            auto targetInstruction = patchup.second;

            if (m_JumpAddresses.find(targetInstruction) == m_JumpAddresses.end())
            {
                throw CompilerException("Internal error. Failed to find address for jump target");
            }

            auto targetAddress = m_JumpAddresses[targetInstruction];

            auto& trigger = *patchup.first;
            auto actionId = GetLastTriggerActionId(trigger);
            if (actionId == -1)
            {
                throw CompilerException("Internal error. Trigger action buffer is full");
            }

            Action_JumpTo(targetAddress, trigger.m_Actions[actionId]);
        }

        if (m_Triggers.back().m_Actions[1].m_ActionType == TriggerActionType::NoAction)
        {
            m_Triggers.pop_back();
        }

        for (auto q = 0u; q < m_HyperTriggerCount; q++)
        {
            Trigger hyperTrigger;
            hyperTrigger.m_ExecutionFlags = 0;
            hyperTrigger.m_ExecutionMask[m_TriggersOwner - 1] = 1;

            Cond_Always(hyperTrigger.m_Conditions[0]);
            Action_PreserveTrigger(hyperTrigger.m_Actions[0]);
            
            for (auto i = 1; i < 64; i++)
            {
                Action_Wait(0, hyperTrigger.m_Actions[i]);
            }

            m_Triggers.push_back(hyperTrigger);
        }

        auto& triggersChunk = chk.GetFirstChunk<CHKTriggersChunk>(ChunkType::TriggersChunk);
        auto oldBytes = triggersChunk.GetBytes();
        auto triggerCount = m_Triggers.size();
        if (preserveTriggers)
        {
            triggerCount += triggersChunk.GetTriggersCount();
        }

        std::vector<char> bytes;
        bytes.resize(triggerCount * sizeof(Trigger));
        memcpy(bytes.data(), m_Triggers.data(), m_Triggers.size() * sizeof(Trigger));

        if (preserveTriggers)
        {
            memcpy(bytes.data() + m_Triggers.size() * sizeof(Trigger), oldBytes.data(), oldBytes.size());
        }

        triggersChunk.SetBytes(bytes);

        return true;
    }

    void Compiler::Cond_Always(TriggerCondition& retCondition)
    {
        using namespace CHK;

        retCondition.m_Condition = TriggerConditionType::Always;
        retCondition.m_Flags = 16;
    }

    unsigned int Compiler::CodeGen_CopyReg(unsigned int dstReg, unsigned int srcReg, unsigned int& nextAddress, unsigned int retAddress)
    {
        using namespace CHK;

        auto copyAddress = nextAddress++;
        auto copy2Address = nextAddress++;

        // step 1 - copy to storage
        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto copyToStorageTrigger = TriggerBuilder(copyAddress, nullptr, m_TriggersOwner);
            copyToStorageTrigger.Cond_TestReg(srcReg, i, TriggerComparisonType::AtLeast);
            copyToStorageTrigger.Action_DecReg(srcReg, i);
            copyToStorageTrigger.Action_IncReg(Reg_CopyStorage, i);
            PushTriggers(copyToStorageTrigger.GetTriggers());
        }

        // step 1 (finish) - finish copy and jump to step 2
        auto finishCopyTrigger = TriggerBuilder(copyAddress, nullptr, m_TriggersOwner);
        finishCopyTrigger.Cond_TestReg(srcReg, 0, TriggerComparisonType::Exactly);
        finishCopyTrigger.Action_SetReg(dstReg, 0);
        finishCopyTrigger.Action_JumpTo(copy2Address);
        PushTriggers(finishCopyTrigger.GetTriggers());

        // step 3 - copy from storage
        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto copyFromStorageTrigger = TriggerBuilder(copy2Address, nullptr, m_TriggersOwner);
            copyFromStorageTrigger.Cond_TestReg(Reg_CopyStorage, i, TriggerComparisonType::AtLeast);
            copyFromStorageTrigger.Action_DecReg(Reg_CopyStorage, i);
            copyFromStorageTrigger.Action_IncReg(srcReg, i);
            copyFromStorageTrigger.Action_IncReg(dstReg, i);
            PushTriggers(copyFromStorageTrigger.GetTriggers());
        }

        // step 3 (finish)
        auto finishCopyFromStorageTrigger = TriggerBuilder(copy2Address, nullptr, m_TriggersOwner);
        finishCopyFromStorageTrigger.Cond_TestReg(Reg_CopyStorage, 0, TriggerComparisonType::Exactly);
        finishCopyFromStorageTrigger.Action_JumpTo(retAddress);
        PushTriggers(finishCopyFromStorageTrigger.GetTriggers());

        return copyAddress;
    }

    void Compiler::Action_PreserveTrigger(TriggerAction& retAction)
    {
        using namespace CHK;
        retAction.m_ActionType = TriggerActionType::PreserveTrigger;
    }

    void Compiler::Action_Wait(unsigned int milliseconds, TriggerAction& retAction)
    {
        using namespace CHK;
        retAction.m_Group = 7;
        retAction.m_ActionType = TriggerActionType::Wait;
        retAction.m_Milliseconds = milliseconds;
        retAction.m_Flags = 16;
    }

    void Compiler::Action_JumpTo(unsigned int address, TriggerAction& retAction)
    {
        using namespace CHK;
        retAction.m_ActionType = TriggerActionType::SetDeaths;
        retAction.m_Modifier = (uint8_t)TriggerActionState::SetTo;

        auto& regDef = g_RegisterMap[Reg_InstructionCounter];

        retAction.m_Flags = 16;
        retAction.m_Group = regDef.m_PlayerId;
        retAction.m_Arg1 = regDef.m_Index;
        retAction.m_Arg0 = address;
    }

    int Compiler::GetLastTriggerActionId(const Trigger& trigger)
    {
        auto index = 0;

        while (trigger.m_Actions[index].m_ActionType != TriggerActionType::NoAction)
        {
            index++;
            if (index >= 64)
            {
                return -1;
            }
        }

        return index;
    }

    unsigned int Compiler::GetLocationIdByName(const std::string& name)
    {
        if (name.length() == 0)
        {
            throw CompilerException("Invalid empty location name");
        }

        if (name == "AnyLocation")
        {
            return 63;
        }

        auto locationStringId = m_StringsChunk->FindString(name);
        if (locationStringId == -1)
        {
            throw CompilerException(SafePrintf("Location \"%\" not found", name));
        }

        auto locationId = m_LocationsChunk->FindLocation(locationStringId + 1);
        if (locationId == -1)
        {
            throw CompilerException(SafePrintf("Location \"%\" not found", name));
        }

        return locationId;
    }

    void Compiler::DoIndirectJump(TriggerBuilder& trigger)
    {
        trigger.Action_SetSwitch(Switch_InstructionCounterMutex, TriggerActionState::SetSwitch);
        trigger.Action_SetReg(Reg_InstructionCounter, 0);
    }

    void Compiler::EmitIndirectJumpCode(unsigned int& nextAddress)
    {
        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto indirectJump = TriggerBuilder(-1, nullptr, m_TriggersOwner);
            indirectJump.Cond_TestSwitch(Switch_InstructionCounterMutex, true);
            indirectJump.Cond_TestReg(Reg_IndirectJumpAddress, i, TriggerComparisonType::AtLeast);
            indirectJump.Action_DecReg(Reg_IndirectJumpAddress, i);
            indirectJump.Action_IncReg(Reg_InstructionCounter, i);
            PushTriggers(indirectJump.GetTriggers());
        }

        auto indirectJumpFinish = TriggerBuilder(-1, nullptr, m_TriggersOwner);
        indirectJumpFinish.Cond_TestSwitch(Switch_InstructionCounterMutex, true);
        indirectJumpFinish.Cond_TestReg(Reg_IndirectJumpAddress, 0, TriggerComparisonType::Exactly);
        indirectJumpFinish.Action_SetSwitch(Switch_InstructionCounterMutex, TriggerActionState::ClearSwitch);
        PushTriggers(indirectJumpFinish.GetTriggers());
    }

    void Compiler::EmitMulInstructionCode(unsigned int& nextAddress)
    {
        auto mulAddress = nextAddress++;
        auto rightToLeftAddress = nextAddress++;
        auto leftToRightAddress = nextAddress++;
        auto checkAddress = nextAddress++;
        auto moveAddress = nextAddress++;
        auto finishAddress = nextAddress++;

        m_MultiplyAddress = nextAddress++;

        auto prepare = TriggerBuilder(m_MultiplyAddress, nullptr, m_TriggersOwner);
        prepare.Action_SetReg(Reg_Temp0, 0);
        prepare.Action_SetReg(Reg_Temp1, 0);
        prepare.Action_JumpTo(mulAddress);
        PushTriggers(prepare.GetTriggers());

        // handle multiply by zero
        auto zeroR = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
        zeroR.Cond_TestReg(Reg_MulRight, 0, TriggerComparisonType::Exactly);
        DoIndirectJump(zeroR);
        PushTriggers(zeroR.GetTriggers());

        auto zeroL = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
        zeroL.Cond_TestReg(Reg_MulLeft, 0, TriggerComparisonType::Exactly);
        zeroL.Action_SetReg(Reg_MulRight, 0);
        DoIndirectJump(zeroL);
        PushTriggers(zeroL.GetTriggers());

        // handle multiply by one
        auto oneL = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
        oneL.Cond_TestReg(Reg_MulLeft, 1, TriggerComparisonType::Exactly);
        DoIndirectJump(oneL);
        PushTriggers(oneL.GetTriggers());

        // count bits
        for (auto i = m_CopyBatchSize; i >= 2; i /= 2)
        {
            auto countBits = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
            countBits.Cond_TestReg(Reg_MulRight, i, TriggerComparisonType::AtLeast);
            countBits.Action_DecReg(Reg_MulRight, i);
            countBits.Action_IncReg(Reg_Temp0, (int)std::log2(i));
            PushTriggers(countBits.GetTriggers());
        }

        auto copyAddress = CodeGen_CopyReg(Reg_Temp2, Reg_MulLeft, nextAddress, checkAddress);

        auto finishCountBits = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
        finishCountBits.Cond_TestReg(Reg_MulRight, 1, TriggerComparisonType::Exactly);
        finishCountBits.Action_SetReg(Reg_Temp2, 0);
        finishCountBits.Action_JumpTo(copyAddress);
        PushTriggers(finishCountBits.GetTriggers());

        auto finishCountBits2 = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
        finishCountBits2.Cond_TestReg(Reg_MulRight, 0, TriggerComparisonType::Exactly);
        finishCountBits2.Action_SetReg(Reg_Temp2, 0);
        finishCountBits2.Action_JumpTo(checkAddress);
        PushTriggers(finishCountBits2.GetTriggers());

        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto shiftA = TriggerBuilder(rightToLeftAddress, nullptr, m_TriggersOwner);
            shiftA.Cond_TestReg(Reg_MulRight, i, TriggerComparisonType::AtLeast);
            shiftA.Action_DecReg(Reg_MulRight, i);
            shiftA.Action_IncReg(Reg_MulLeft, i * 2);
            PushTriggers(shiftA.GetTriggers());
        }

        auto shiftAFinish = TriggerBuilder(rightToLeftAddress, nullptr, m_TriggersOwner);
        shiftAFinish.Cond_TestReg(Reg_MulRight, 0, TriggerComparisonType::Exactly);
        shiftAFinish.Action_SetReg(Reg_Temp1, 0);
        shiftAFinish.Action_JumpTo(checkAddress);
        PushTriggers(shiftAFinish.GetTriggers());

        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto shiftB = TriggerBuilder(leftToRightAddress, nullptr, m_TriggersOwner);
            shiftB.Cond_TestReg(Reg_MulLeft, i, TriggerComparisonType::AtLeast);
            shiftB.Action_DecReg(Reg_MulLeft, i);
            shiftB.Action_IncReg(Reg_MulRight, i * 2);
            PushTriggers(shiftB.GetTriggers());
        }

        auto shiftBFinish = TriggerBuilder(leftToRightAddress, nullptr, m_TriggersOwner);
        shiftBFinish.Cond_TestReg(Reg_MulLeft, 0, TriggerComparisonType::Exactly);
        shiftBFinish.Action_SetReg(Reg_Temp1, 1);
        shiftBFinish.Action_JumpTo(checkAddress);
        PushTriggers(shiftBFinish.GetTriggers());

        for (auto i = m_CopyBatchSize; i >= 1; i /= 2) // next we'll use the counter from step 1 to multiply the other operand that many times by 2
        {
            auto move = TriggerBuilder(moveAddress, nullptr, m_TriggersOwner);
            move.Cond_TestReg(Reg_MulLeft, i, TriggerComparisonType::AtLeast);
            move.Action_DecReg(Reg_MulLeft, i);
            move.Action_IncReg(Reg_MulRight, i);
            PushTriggers(move.GetTriggers());
        }

        auto moveFinish = TriggerBuilder(moveAddress, nullptr, m_TriggersOwner);
        moveFinish.Cond_TestReg(Reg_MulLeft, 0, TriggerComparisonType::Exactly);
        moveFinish.Action_JumpTo(finishAddress);
        PushTriggers(moveFinish.GetTriggers());

        auto checkA = TriggerBuilder(checkAddress, nullptr, m_TriggersOwner);
        checkA.Cond_TestReg(Reg_Temp0, 0, TriggerComparisonType::Exactly);
        checkA.Cond_TestReg(Reg_Temp1, 0, TriggerComparisonType::Exactly);
        checkA.Action_JumpTo(moveAddress);
        PushTriggers(checkA.GetTriggers());

        auto checkB = TriggerBuilder(checkAddress, nullptr, m_TriggersOwner);
        checkB.Cond_TestReg(Reg_Temp0, 0, TriggerComparisonType::Exactly);
        checkB.Cond_TestReg(Reg_Temp1, 1, TriggerComparisonType::Exactly);
        checkB.Action_JumpTo(finishAddress);
        PushTriggers(checkB.GetTriggers());

        auto checkNotDoneA = TriggerBuilder(checkAddress, nullptr, m_TriggersOwner);
        checkNotDoneA.Cond_TestReg(Reg_Temp0, 1, TriggerComparisonType::AtLeast);
        checkNotDoneA.Cond_TestReg(Reg_Temp1, 0, TriggerComparisonType::Exactly);
        checkNotDoneA.Action_DecReg(Reg_Temp0, 1);
        checkNotDoneA.Action_SetReg(Reg_MulRight, 0);
        checkNotDoneA.Action_JumpTo(leftToRightAddress);
        PushTriggers(checkNotDoneA.GetTriggers());

        auto checkNotDoneB = TriggerBuilder(checkAddress, nullptr, m_TriggersOwner);
        checkNotDoneB.Cond_TestReg(Reg_Temp0, 1, TriggerComparisonType::AtLeast);
        checkNotDoneB.Cond_TestReg(Reg_Temp1, 1, TriggerComparisonType::Exactly);
        checkNotDoneB.Action_DecReg(Reg_Temp0, 1);
        checkNotDoneB.Action_SetReg(Reg_MulLeft, 0);
        checkNotDoneB.Action_JumpTo(rightToLeftAddress);
        PushTriggers(checkNotDoneB.GetTriggers());

        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto finish = TriggerBuilder(finishAddress, nullptr, m_TriggersOwner);
            finish.Cond_TestReg(Reg_Temp2, i, TriggerComparisonType::AtLeast);
            finish.Action_DecReg(Reg_Temp2, i);
            finish.Action_IncReg(Reg_MulRight, i);
            PushTriggers(finish.GetTriggers());
        }

        auto finishMul = TriggerBuilder(finishAddress, nullptr, m_TriggersOwner);
        finishMul.Cond_TestReg(Reg_Temp2, 0, TriggerComparisonType::Exactly);
        DoIndirectJump(finishMul);
        PushTriggers(finishMul.GetTriggers());
    }

}
