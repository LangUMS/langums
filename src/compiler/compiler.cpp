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

        m_File = &chk;
        m_StringsChunk = &chk.GetFirstChunk<CHKStringsChunk>("STR ");
        if (m_StringsChunk == nullptr)
        {
            throw CompilerException("No strings chunk found in scenario file");
        }
         
        m_LocationsChunk = &chk.GetFirstChunk<CHKLocationsChunk>("MRGN");
        if (m_LocationsChunk == nullptr)
        {
            throw CompilerException("No locations chunk found in scenario file");
        }

        m_CuwpChunk = &chk.GetFirstChunk<CHKCuwpChunk>("UPRP");
        if (m_CuwpChunk == nullptr)
        {
            throw CompilerException("No CUWP slots chunk found in scenario file");
        }

        m_CuwpUsedChunk = &chk.GetFirstChunk<CHKCuwpUsedChunk>("UPUS");

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
                        eventTrigger.Cond_Opponents(opponents->GetPlayerId(), (TriggerComparisonType)opponents->GetComparison(), opponents->GetQuantity());
                    }
                }

                m_Triggers.push_back(eventTrigger.GetTrigger());
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
            else if (type == IRInstructionType::JmpIfEqZero)
            {
                auto jmp = (IRJmpIfEqZeroInstruction*)instruction.get();
                targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
            }
            else if (type == IRInstructionType::JmpIfNotEqZero)
            {
                auto jmp = (IRJmpIfNotEqZeroInstruction*)instruction.get();
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

        std::unordered_map<Trigger*, IIRInstruction*> jmpPatchups;

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
                    m_Triggers.push_back(current.GetTrigger());
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
                    m_Triggers.push_back(current.GetTrigger());
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
                    m_Triggers.push_back(current.GetTrigger());

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto copy = TriggerBuilder(copyAddress, instruction.get(), m_TriggersOwner);
                        copy.Cond_TestReg(stackTop, i, TriggerComparisonType::AtLeast);
                        copy.Action_DecReg(stackTop, i);
                        copy.Action_IncReg(regId, i);
                        m_Triggers.push_back(copy.GetTrigger());
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

                m_Triggers.push_back(current.GetTrigger());
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
            }
            else if (instruction->GetType() == IRInstructionType::Add)
            {
                auto addAddress = nextAddress++;
                current.Action_JumpTo(addAddress);

                m_Triggers.push_back(current.GetTrigger());
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
                    m_Triggers.push_back(add.GetTrigger());
                }

                auto finishAdd = TriggerBuilder(addAddress, instruction.get(), m_TriggersOwner);
                finishAdd.Cond_TestReg(left, 0, TriggerComparisonType::Exactly);
                finishAdd.Action_JumpTo(retAddress);
                m_Triggers.push_back(finishAdd.GetTrigger());
            }
            else if (instruction->GetType() == IRInstructionType::Sub)
            {
                auto subAddress = nextAddress++;
                current.Action_SetSwitch(Switch_ArithmeticUnderflow, TriggerActionState::ClearSwitch);
                current.Action_JumpTo(subAddress);

                m_Triggers.push_back(current.GetTrigger());
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
                    m_Triggers.push_back(sub.GetTrigger());
                }

                auto finishSub = TriggerBuilder(subAddress, instruction.get(), m_TriggersOwner);
                finishSub.Cond_TestReg(left, 0, TriggerComparisonType::Exactly);
                finishSub.Action_SetSwitch(Switch_ArithmeticUnderflow, TriggerActionState::ClearSwitch);
                finishSub.Action_JumpTo(retAddress);
                m_Triggers.push_back(finishSub.GetTrigger());

                finishSub = TriggerBuilder(subAddress, instruction.get(), m_TriggersOwner);
                finishSub.Cond_TestReg(left, 1, TriggerComparisonType::AtLeast);
                finishSub.Cond_TestReg(right, 0, TriggerComparisonType::Exactly);
                finishSub.Action_SetSwitch(Switch_ArithmeticUnderflow, TriggerActionState::SetSwitch);
                finishSub.Action_JumpTo(retAddress);
                m_Triggers.push_back(finishSub.GetTrigger());
            }
            else if (instruction->GetType() == IRInstructionType::Mul)
            {
                auto mulAddress = nextAddress++;
                auto rightToLeftAddress = nextAddress++;
                auto leftToRightAddress = nextAddress++;
                auto checkAddress = nextAddress++;
                auto moveAddress = nextAddress++;
                auto finishAddress = nextAddress++;

                current.Action_SetReg(Reg_Temp0, 0);
                current.Action_SetReg(Reg_Temp1, 0);

                current.Action_JumpTo(mulAddress);
                m_Triggers.push_back(current.GetTrigger());

                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                auto left = ++m_StackPointer;
                auto right = m_StackPointer + 1;

                // handle multiply by zero
                auto zeroR = TriggerBuilder(mulAddress, instruction.get(), m_TriggersOwner);
                zeroR.Cond_TestReg(right, 0, TriggerComparisonType::Exactly);
                zeroR.Action_JumpTo(retAddress);
                m_Triggers.push_back(zeroR.GetTrigger());

                auto zeroL = TriggerBuilder(mulAddress, instruction.get(), m_TriggersOwner);
                zeroL.Cond_TestReg(left, 0, TriggerComparisonType::Exactly);
                zeroL.Action_SetReg(right, 0);
                zeroL.Action_JumpTo(retAddress);
                m_Triggers.push_back(zeroL.GetTrigger());

                // handle multiply by one
                auto oneL = TriggerBuilder(mulAddress, instruction.get(), m_TriggersOwner);
                oneL.Cond_TestReg(left, 1, TriggerComparisonType::Exactly);
                oneL.Action_JumpTo(retAddress);
                m_Triggers.push_back(oneL.GetTrigger());

                // count bits
                for (auto i = m_CopyBatchSize; i >= 2; i /= 2)
                {
                    auto countBits = TriggerBuilder(mulAddress, instruction.get(), m_TriggersOwner);
                    countBits.Cond_TestReg(right, i, TriggerComparisonType::AtLeast);
                    countBits.Action_DecReg(right, i);
                    countBits.Action_IncReg(Reg_Temp0, (int)std::log2(i));
                    m_Triggers.push_back(countBits.GetTrigger());
                }

                auto copyAddress = CodeGen_CopyReg(Reg_Temp2, left, nextAddress, checkAddress);

                auto finishCountBits = TriggerBuilder(mulAddress, instruction.get(), m_TriggersOwner);
                finishCountBits.Cond_TestReg(right, 1, TriggerComparisonType::Exactly);
                finishCountBits.Action_SetReg(Reg_Temp2, 0);
                finishCountBits.Action_JumpTo(copyAddress);
                m_Triggers.push_back(finishCountBits.GetTrigger());

                auto finishCountBits2 = TriggerBuilder(mulAddress, instruction.get(), m_TriggersOwner);
                finishCountBits2.Cond_TestReg(right, 0, TriggerComparisonType::Exactly);
                finishCountBits2.Action_SetReg(Reg_Temp2, 0);
                finishCountBits2.Action_JumpTo(checkAddress);
                m_Triggers.push_back(finishCountBits2.GetTrigger());

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2) 
                {
                    auto shiftA = TriggerBuilder(rightToLeftAddress, instruction.get(), m_TriggersOwner);
                    shiftA.Cond_TestReg(right, i, TriggerComparisonType::AtLeast);
                    shiftA.Action_DecReg(right, i);
                    shiftA.Action_IncReg(left, i * 2);
                    m_Triggers.push_back(shiftA.GetTrigger());
                }

                auto shiftAFinish = TriggerBuilder(rightToLeftAddress, instruction.get(), m_TriggersOwner);
                shiftAFinish.Cond_TestReg(right, 0, TriggerComparisonType::Exactly);
                shiftAFinish.Action_SetReg(Reg_Temp1, 0);
                shiftAFinish.Action_JumpTo(checkAddress);
                m_Triggers.push_back(shiftAFinish.GetTrigger());

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto shiftB = TriggerBuilder(leftToRightAddress, instruction.get(), m_TriggersOwner);
                    shiftB.Cond_TestReg(left, i, TriggerComparisonType::AtLeast);
                    shiftB.Action_DecReg(left, i);
                    shiftB.Action_IncReg(right, i * 2);
                    m_Triggers.push_back(shiftB.GetTrigger());
                }

                auto shiftBFinish = TriggerBuilder(leftToRightAddress, instruction.get(), m_TriggersOwner);
                shiftBFinish.Cond_TestReg(left, 0, TriggerComparisonType::Exactly);
                shiftBFinish.Action_SetReg(Reg_Temp1, 1);
                shiftBFinish.Action_JumpTo(checkAddress);
                m_Triggers.push_back(shiftBFinish.GetTrigger());

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2) // next we'll use the counter from step 1 to multiply the other operand that many times by 2
                {
                    auto move = TriggerBuilder(moveAddress, instruction.get(), m_TriggersOwner);
                    move.Cond_TestReg(left, i, TriggerComparisonType::AtLeast);
                    move.Action_DecReg(left, i);
                    move.Action_IncReg(right, i);
                    m_Triggers.push_back(move.GetTrigger());
                }

                auto moveFinish = TriggerBuilder(moveAddress, instruction.get(), m_TriggersOwner);
                moveFinish.Cond_TestReg(left, 0, TriggerComparisonType::Exactly);
                moveFinish.Action_JumpTo(finishAddress);
                m_Triggers.push_back(moveFinish.GetTrigger());

                auto checkA = TriggerBuilder(checkAddress, instruction.get(), m_TriggersOwner);
                checkA.Cond_TestReg(Reg_Temp0, 0, TriggerComparisonType::Exactly);
                checkA.Cond_TestReg(Reg_Temp1, 0, TriggerComparisonType::Exactly);
                checkA.Action_JumpTo(moveAddress);
                m_Triggers.push_back(checkA.GetTrigger());

                auto checkB = TriggerBuilder(checkAddress, instruction.get(), m_TriggersOwner);
                checkB.Cond_TestReg(Reg_Temp0, 0, TriggerComparisonType::Exactly);
                checkB.Cond_TestReg(Reg_Temp1, 1, TriggerComparisonType::Exactly);
                checkB.Action_JumpTo(finishAddress);
                m_Triggers.push_back(checkB.GetTrigger());

                auto checkNotDoneA = TriggerBuilder(checkAddress, instruction.get(), m_TriggersOwner);
                checkNotDoneA.Cond_TestReg(Reg_Temp0, 1, TriggerComparisonType::AtLeast);
                checkNotDoneA.Cond_TestReg(Reg_Temp1, 0, TriggerComparisonType::Exactly);
                checkNotDoneA.Action_DecReg(Reg_Temp0, 1);
                checkNotDoneA.Action_SetReg(right, 0);
                checkNotDoneA.Action_JumpTo(leftToRightAddress);
                m_Triggers.push_back(checkNotDoneA.GetTrigger());

                auto checkNotDoneB = TriggerBuilder(checkAddress, instruction.get(), m_TriggersOwner);
                checkNotDoneB.Cond_TestReg(Reg_Temp0, 1, TriggerComparisonType::AtLeast);
                checkNotDoneB.Cond_TestReg(Reg_Temp1, 1, TriggerComparisonType::Exactly);
                checkNotDoneB.Action_DecReg(Reg_Temp0, 1);
                checkNotDoneB.Action_SetReg(left, 0);
                checkNotDoneB.Action_JumpTo(rightToLeftAddress);
                m_Triggers.push_back(checkNotDoneB.GetTrigger());

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2) 
                {
                    auto finish = TriggerBuilder(finishAddress, instruction.get(), m_TriggersOwner);
                    finish.Cond_TestReg(Reg_Temp2, i, TriggerComparisonType::AtLeast);
                    finish.Action_DecReg(Reg_Temp2, i);
                    finish.Action_IncReg(right, i);
                    m_Triggers.push_back(finish.GetTrigger());
                }

                auto finishMul = TriggerBuilder(finishAddress, instruction.get(), m_TriggersOwner);
                finishMul.Cond_TestReg(Reg_Temp2, 0, TriggerComparisonType::Exactly);
                finishMul.Action_JumpTo(retAddress);
                m_Triggers.push_back(finishMul.GetTrigger());
            }
            else if (instruction->GetType() == IRInstructionType::Div)
            {
                throw CompilerException("Malformed IR. Native Div is not implemented yet");
            }
            else if (instruction->GetType() == IRInstructionType::BLShift)
            {
                auto lshift = (IRBLShiftInstruction*)instruction.get();
                auto numBits = lshift->GetNumBits();

                auto left = ++m_StackPointer;
                auto right = m_StackPointer + 1;

                auto shiftAddress = nextAddress++;
                current.Action_SetReg(right, 0);
                current.Action_JumpTo(shiftAddress);

                m_Triggers.push_back(current.GetTrigger());
                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto shift = TriggerBuilder(shiftAddress, instruction.get(), m_TriggersOwner);
                    shift.Cond_TestReg(left, i, TriggerComparisonType::AtLeast);
                    shift.Action_DecReg(left, i);
                    shift.Action_IncReg(right, i * (1 << numBits));
                    m_Triggers.push_back(shift.GetTrigger());
                }

                auto finishShift = TriggerBuilder(shiftAddress, instruction.get(), m_TriggersOwner);
                finishShift.Cond_TestReg(left, 0, TriggerComparisonType::Exactly);
                finishShift.Action_JumpTo(retAddress);
                m_Triggers.push_back(finishShift.GetTrigger());
            }
            else if (instruction->GetType() == IRInstructionType::BRShift)
            {
                auto rshift = (IRBRShiftInstruction*)instruction.get();
                auto numBits = rshift->GetNumBits();

                auto left = ++m_StackPointer;
                auto right = m_StackPointer + 1;

                auto shiftAddress = nextAddress++;
                current.Action_SetReg(right, 0);
                current.Action_JumpTo(shiftAddress);

                m_Triggers.push_back(current.GetTrigger());
                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto shift = TriggerBuilder(shiftAddress, instruction.get(), m_TriggersOwner);
                    shift.Cond_TestReg(left, i, TriggerComparisonType::AtLeast);
                    shift.Action_DecReg(left, i);
                    shift.Action_IncReg(right, i / (1 << numBits));
                    m_Triggers.push_back(shift.GetTrigger());
                }

                auto finishShift = TriggerBuilder(shiftAddress, instruction.get(), m_TriggersOwner);
                finishShift.Cond_TestReg(left, 0, TriggerComparisonType::Exactly);
                finishShift.Action_JumpTo(retAddress);
                m_Triggers.push_back(finishShift.GetTrigger());
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

                m_Triggers.push_back(current.GetTrigger());
                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                for (auto i = 0; i < 8; i++)
                {
                    auto rnd = TriggerBuilder(rndAddress, instruction.get(), m_TriggersOwner);
                    rnd.Cond_TestSwitch(Switch_Random0 + i, true);
                    rnd.Action_IncReg(stackTop, (1 << i));
                    m_Triggers.push_back(rnd.GetTrigger());
                }

                auto finish = TriggerBuilder(rndAddress, instruction.get(), m_TriggersOwner);
                finish.Action_JumpTo(retAddress);
                m_Triggers.push_back(finish.GetTrigger());
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

                m_Triggers.push_back(current.GetTrigger());
                auto trigger = &m_Triggers.back();
                current = TriggerBuilder(nextAddress++, instruction.get(), m_TriggersOwner);

                jmpPatchups.insert(std::make_pair(trigger, targetInstruction));
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfEqZero)
            {
                auto jmp = (IRJmpIfEqZeroInstruction*)instruction.get();

                auto regId = jmp->GetRegisterId();

                if (regId > Reg_StackTop)
                {
                    throw CompilerException("Malformed IR. Testing past top of the stack");
                }

                if (regId == Reg_StackTop)
                {
                    regId = ++m_StackPointer;
                }

                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto retAddress = nextAddress++;

                auto ifFalse = current;

                current.Cond_TestReg(regId, 1, TriggerComparisonType::AtLeast);
                current.Action_JumpTo(retAddress);
                m_Triggers.push_back(current.GetTrigger());

                ifFalse.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                m_Triggers.push_back(ifFalse.GetTrigger());
                auto trigger = &m_Triggers.back();
                jmpPatchups.insert(std::make_pair(trigger, instructions[targetIndex].get()));

                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfNotEqZero)
            {
                auto jmp = (IRJmpIfNotEqZeroInstruction*)instruction.get();

                auto regId = jmp->GetRegisterId();
                if (regId > Reg_StackTop)
                {
                    throw CompilerException("Malformed IR. Testing past top of the stack");
                }

                if (regId == Reg_StackTop)
                {
                    regId = ++m_StackPointer;
                }

                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto retAddress = nextAddress++;
                auto ifFalse = current;

                current.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                current.Action_JumpTo(retAddress);
                m_Triggers.push_back(current.GetTrigger());

                ifFalse.Cond_TestReg(regId, 1, TriggerComparisonType::AtLeast);
                m_Triggers.push_back(ifFalse.GetTrigger());
                auto trigger = &m_Triggers.back();
                jmpPatchups.insert(std::make_pair(trigger, instructions[targetIndex].get()));

                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfSwNotSet)
            {
                auto jmp = (IRJmpIfSwNotSetInstruction*)instruction.get();
                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto targetInstruction = instructions[targetIndex].get();;

                auto switchId = jmp->GetSwitchId();

                auto retAddress = nextAddress++;
                auto ifFalse = current;

                current.Cond_TestSwitch(switchId, true);
                current.Action_JumpTo(retAddress);
                m_Triggers.push_back(current.GetTrigger());

                ifFalse.Cond_TestSwitch(switchId, false);
                m_Triggers.push_back(ifFalse.GetTrigger());
                auto trigger = &m_Triggers.back();
                jmpPatchups.insert(std::make_pair(trigger, targetInstruction));

                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfSwSet)
            {
                auto jmp = (IRJmpIfSwSetInstruction*)instruction.get();
                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                if (targetIndex >= (int)instructions.size())
                {
                    targetIndex = (int)instructions.size() - 1;
                }

                auto targetInstruction = instructions[targetIndex].get();;

                auto switchId = jmp->GetSwitchId();

                auto retAddress = nextAddress++;
                auto ifFalse = current;

                current.Cond_TestSwitch(switchId, false);
                current.Action_JumpTo(retAddress);
                m_Triggers.push_back(current.GetTrigger());

                ifFalse.Cond_TestSwitch(switchId, true);
                m_Triggers.push_back(ifFalse.GetTrigger());
                auto trigger = &m_Triggers.back();
                jmpPatchups.insert(std::make_pair(trigger, targetInstruction));

                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
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
                m_Triggers.push_back(current.GetTrigger());
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
                    m_Triggers.push_back(checkIfPlayerActive.GetTrigger());
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
                m_Triggers.push_back(current.GetTrigger());

                auto checkPresent = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                checkPresent.Action_Wait(0);
                checkPresent.Action_JumpTo(retAddress);
                m_Triggers.push_back(checkPresent.GetTrigger());

                for (auto& playerId : playerIds)
                {
                    auto countPresent = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    countPresent.Cond_TestSwitch(Switch_Player1 + playerId, true);
                    countPresent.Action_IncReg(stackTop, 1);
                    m_Triggers.push_back(countPresent.GetTrigger());
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
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    m_Triggers.push_back(waitTrigger.GetTrigger());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner;
                    }

                    auto msgTrigger = TriggerBuilder(address, instruction.get(), playerId);
                    msgTrigger.Action_DisplayMsg(stringId);

                    if (all)
                    {
                        msgTrigger.SetExecuteForAllPlayers();
                    }

                    m_Triggers.push_back(msgTrigger.GetTrigger());

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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto spawnTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        spawnTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        spawnTrigger.Action_DecReg(regId, i);
                        spawnTrigger.Action_CreateUnit(spawn->GetPlayerId(), spawn->GetUnitId(), i, locationId, unitSlot);
                        m_Triggers.push_back(spawnTrigger.GetTrigger());
                    }

                    auto finishSpawn = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishSpawn.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishSpawn.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishSpawn.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto killTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        killTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        killTrigger.Action_DecReg(regId, i);
                        killTrigger.Action_KillUnit(kill->GetPlayerId(), kill->GetUnitId(), i, locationId);
                        m_Triggers.push_back(killTrigger.GetTrigger());
                    }

                    auto finishTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishTrigger.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishTrigger.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishTrigger.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto removeTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        removeTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        removeTrigger.Action_DecReg(regId, i);
                        removeTrigger.Action_RemoveUnit(remove->GetPlayerId(), remove->GetUnitId(), i, locationId);
                        m_Triggers.push_back(removeTrigger.GetTrigger());
                    }

                    auto finishTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishTrigger.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishTrigger.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishTrigger.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto moveTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        moveTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        moveTrigger.Action_DecReg(regId, i);
                        current.Action_MoveUnit(move->GetPlayerId(), move->GetUnitId(), i, srcLocationId, dstLocationId);
                        m_Triggers.push_back(moveTrigger.GetTrigger());
                    }

                    auto finishTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishTrigger.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishTrigger.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishTrigger.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto modifyTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
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

                        m_Triggers.push_back(modifyTrigger.GetTrigger());
                    }

                    auto finishModify = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishModify.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishModify.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishModify.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto giveTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        giveTrigger.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        giveTrigger.Action_DecReg(regId, i);
                        giveTrigger.Action_GiveUnits(srcPlayerId, dstPlayerId, unitId, i, locationId);
                        m_Triggers.push_back(giveTrigger.GetTrigger());
                    }

                    auto finishGive = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishGive.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishGive.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishGive.GetTrigger());
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
                auto playerMask = endGame->GetPlayerMask();

                auto address = nextAddress++;
                current.Action_JumpTo(address);
                m_Triggers.push_back(current.GetTrigger());

                auto endGameTrigger = TriggerBuilder(address, instruction.get(), playerMask);

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

                auto retAddress = nextAddress++;
                endGameTrigger.Action_JumpTo(retAddress);
                m_Triggers.push_back(endGameTrigger.GetTrigger());

                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
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
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    m_Triggers.push_back(waitTrigger.GetTrigger());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner;
                    }

                    auto centerTrigger = TriggerBuilder(address, instruction.get(), playerId);
                    centerTrigger.Action_CenterView(locationId + 1);

                    if (all)
                    {
                        centerTrigger.SetExecuteForAllPlayers();
                    }

                    m_Triggers.push_back(centerTrigger.GetTrigger());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::Ping)
            {
                auto ping = (IRPingInstruction*)instruction.get();
                auto playerMask = ping->GetPlayerId();

                auto locationId = GetLocationIdByName(ping->GetLocationName());

                auto address = nextAddress++;
                current.Action_JumpTo(address);
                m_Triggers.push_back(current.GetTrigger());

                auto pingTrigger = TriggerBuilder(address, instruction.get(), playerMask + 1);

                pingTrigger.Action_Ping(locationId + 1);

                auto retAddress = nextAddress++;
                pingTrigger.Action_JumpTo(retAddress);
                m_Triggers.push_back(pingTrigger.GetTrigger());

                current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
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
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        add.Action_SetResources(playerId, i, TriggerActionState::Add, setResource->GetResourceType());
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        add.Action_SetResources(playerId, i, TriggerActionState::Add, setResource->GetResourceType());
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto sub = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        sub.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        sub.Action_DecReg(regId, i);
                        sub.Action_SetResources(playerId, i, TriggerActionState::Subtract, setResource->GetResourceType());
                        m_Triggers.push_back(sub.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
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
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetScore(playerId, i, TriggerActionState::Add, setScore->GetScoreType());
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetScore(playerId, i, TriggerActionState::Add, incScore->GetScoreType());
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetScore(playerId, i, TriggerActionState::Subtract, incScore->GetScoreType());
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
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
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        add.Action_SetCountdown(i, TriggerActionState::Add);
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
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
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetDeaths(playerId, setDeaths->GetUnitId(), i, TriggerActionState::Add);
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetDeaths(playerId, incDeaths->GetUnitId(), i, TriggerActionState::Add);
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
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

                    auto address = nextAddress++;
                    current.Action_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                        add.Cond_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.Action_DecReg(regId, i);
                        current.Action_SetDeaths(playerId, decDeaths->GetUnitId(), i, TriggerActionState::Subtract);
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    finishAdd.Cond_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.Action_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
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
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    m_Triggers.push_back(waitTrigger.GetTrigger());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner;
                    }

                    auto talkTrigger = TriggerBuilder(address, instruction.get(), playerId);
                    talkTrigger.Action_TalkingPortrait(talk->GetUnitId(), talk->GetTime());

                    if (all)
                    {
                        talkTrigger.SetExecuteForAllPlayers();
                    }

                    m_Triggers.push_back(talkTrigger.GetTrigger());

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
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    m_Triggers.push_back(waitTrigger.GetTrigger());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner;
                    }

                    auto aiTrigger = TriggerBuilder(address, instruction.get(), playerId);
                    aiTrigger.Action_RunAIScript(playerId, aiScript->GetScriptName(), locationId);

                    if (all)
                    {
                        aiTrigger.SetExecuteForAllPlayers();
                    }

                    m_Triggers.push_back(aiTrigger.GetTrigger());

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
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    m_Triggers.push_back(waitTrigger.GetTrigger());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner;
                    }

                    auto allyTrigger = TriggerBuilder(address, instruction.get(), playerId);
                    allyTrigger.Action_SetAllianceStatus(playerId, targetPlayerId, setAlly->GetAllianceStatus());

                    if (all)
                    {
                        allyTrigger.SetExecuteForAllPlayers();
                    }

                    m_Triggers.push_back(allyTrigger.GetTrigger());

                    current = TriggerBuilder(retAddress, instruction.get(), m_TriggersOwner);
                }
            }
            else if (instruction->GetType() == IRInstructionType::SetObj)
            {
                auto setObj = (IRSetObjInstruction*)instruction.get();
                auto stringId = m_StringsChunk->InsertString(setObj->GetText());
                current.Action_SetMissionObjectives(stringId);
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
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    auto waitTrigger = TriggerBuilder(address, instruction.get(), m_TriggersOwner);
                    waitTrigger.Action_Wait(0);
                    waitTrigger.Action_JumpTo(retAddress);
                    m_Triggers.push_back(waitTrigger.GetTrigger());

                    auto all = false;
                    if (playerId == -1 || playerId == (int)CHK::PlayerId::AllPlayers)
                    {
                        all = true;
                        playerId = m_TriggersOwner;
                    }

                    auto wavTrigger = TriggerBuilder(address, instruction.get(), playerId);
                    wavTrigger.Action_PlayWAV(wavStringId, wavTime);

                    if (all)
                    {
                        wavTrigger.SetExecuteForAllPlayers();
                    }

                    m_Triggers.push_back(wavTrigger.GetTrigger());

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

        m_Triggers.push_back(current.GetTrigger());

        for (auto& patchup : jmpPatchups)
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

        auto& triggersChunk = chk.GetFirstChunk<CHKTriggersChunk>("TRIG");
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
            m_Triggers.push_back(copyToStorageTrigger.GetTrigger());
        }

        // step 1 (finish) - finish copy and jump to step 2
        auto finishCopyTrigger = TriggerBuilder(copyAddress, nullptr, m_TriggersOwner);
        finishCopyTrigger.Cond_TestReg(srcReg, 0, TriggerComparisonType::Exactly);
        finishCopyTrigger.Action_SetReg(dstReg, 0);
        finishCopyTrigger.Action_JumpTo(copy2Address);
        m_Triggers.push_back(finishCopyTrigger.GetTrigger());

        // step 3 - copy from storage
        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto copyFromStorageTrigger = TriggerBuilder(copy2Address, nullptr, m_TriggersOwner);
            copyFromStorageTrigger.Cond_TestReg(Reg_CopyStorage, i, TriggerComparisonType::AtLeast);
            copyFromStorageTrigger.Action_DecReg(Reg_CopyStorage, i);
            copyFromStorageTrigger.Action_IncReg(srcReg, i);
            copyFromStorageTrigger.Action_IncReg(dstReg, i);
            m_Triggers.push_back(copyFromStorageTrigger.GetTrigger());
        }

        // step 3 (finish)
        auto finishCopyFromStorageTrigger = TriggerBuilder(copy2Address, nullptr, m_TriggersOwner);
        finishCopyFromStorageTrigger.Cond_TestReg(Reg_CopyStorage, 0, TriggerComparisonType::Exactly);
        finishCopyFromStorageTrigger.Action_JumpTo(retAddress);
        m_Triggers.push_back(finishCopyFromStorageTrigger.GetTrigger());

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
            m_Triggers.push_back(indirectJump.GetTrigger());
        }

        auto indirectJumpFinish = TriggerBuilder(-1, nullptr, m_TriggersOwner);
        indirectJumpFinish.Cond_TestSwitch(Switch_InstructionCounterMutex, true);
        indirectJumpFinish.Cond_TestReg(Reg_IndirectJumpAddress, 0, TriggerComparisonType::Exactly);
        indirectJumpFinish.Action_SetSwitch(Switch_InstructionCounterMutex, TriggerActionState::ClearSwitch);
        m_Triggers.push_back(indirectJumpFinish.GetTrigger());
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
        m_Triggers.push_back(prepare.GetTrigger());

        // handle multiply by zero
        auto zeroR = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
        zeroR.Cond_TestReg(Reg_MulRight, 0, TriggerComparisonType::Exactly);
        DoIndirectJump(zeroR);
        m_Triggers.push_back(zeroR.GetTrigger());

        auto zeroL = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
        zeroL.Cond_TestReg(Reg_MulLeft, 0, TriggerComparisonType::Exactly);
        zeroL.Action_SetReg(Reg_MulRight, 0);
        DoIndirectJump(zeroL);
        m_Triggers.push_back(zeroL.GetTrigger());

        // handle multiply by one
        auto oneL = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
        oneL.Cond_TestReg(Reg_MulLeft, 1, TriggerComparisonType::Exactly);
        DoIndirectJump(oneL);
        m_Triggers.push_back(oneL.GetTrigger());

        // count bits
        for (auto i = m_CopyBatchSize; i >= 2; i /= 2)
        {
            auto countBits = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
            countBits.Cond_TestReg(Reg_MulRight, i, TriggerComparisonType::AtLeast);
            countBits.Action_DecReg(Reg_MulRight, i);
            countBits.Action_IncReg(Reg_Temp0, (int)std::log2(i));
            m_Triggers.push_back(countBits.GetTrigger());
        }

        auto copyAddress = CodeGen_CopyReg(Reg_Temp2, Reg_MulLeft, nextAddress, checkAddress);

        auto finishCountBits = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
        finishCountBits.Cond_TestReg(Reg_MulRight, 1, TriggerComparisonType::Exactly);
        finishCountBits.Action_SetReg(Reg_Temp2, 0);
        finishCountBits.Action_JumpTo(copyAddress);
        m_Triggers.push_back(finishCountBits.GetTrigger());

        auto finishCountBits2 = TriggerBuilder(mulAddress, nullptr, m_TriggersOwner);
        finishCountBits2.Cond_TestReg(Reg_MulRight, 0, TriggerComparisonType::Exactly);
        finishCountBits2.Action_SetReg(Reg_Temp2, 0);
        finishCountBits2.Action_JumpTo(checkAddress);
        m_Triggers.push_back(finishCountBits2.GetTrigger());

        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto shiftA = TriggerBuilder(rightToLeftAddress, nullptr, m_TriggersOwner);
            shiftA.Cond_TestReg(Reg_MulRight, i, TriggerComparisonType::AtLeast);
            shiftA.Action_DecReg(Reg_MulRight, i);
            shiftA.Action_IncReg(Reg_MulLeft, i * 2);
            m_Triggers.push_back(shiftA.GetTrigger());
        }

        auto shiftAFinish = TriggerBuilder(rightToLeftAddress, nullptr, m_TriggersOwner);
        shiftAFinish.Cond_TestReg(Reg_MulRight, 0, TriggerComparisonType::Exactly);
        shiftAFinish.Action_SetReg(Reg_Temp1, 0);
        shiftAFinish.Action_JumpTo(checkAddress);
        m_Triggers.push_back(shiftAFinish.GetTrigger());

        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto shiftB = TriggerBuilder(leftToRightAddress, nullptr, m_TriggersOwner);
            shiftB.Cond_TestReg(Reg_MulLeft, i, TriggerComparisonType::AtLeast);
            shiftB.Action_DecReg(Reg_MulLeft, i);
            shiftB.Action_IncReg(Reg_MulRight, i * 2);
            m_Triggers.push_back(shiftB.GetTrigger());
        }

        auto shiftBFinish = TriggerBuilder(leftToRightAddress, nullptr, m_TriggersOwner);
        shiftBFinish.Cond_TestReg(Reg_MulLeft, 0, TriggerComparisonType::Exactly);
        shiftBFinish.Action_SetReg(Reg_Temp1, 1);
        shiftBFinish.Action_JumpTo(checkAddress);
        m_Triggers.push_back(shiftBFinish.GetTrigger());

        for (auto i = m_CopyBatchSize; i >= 1; i /= 2) // next we'll use the counter from step 1 to multiply the other operand that many times by 2
        {
            auto move = TriggerBuilder(moveAddress, nullptr, m_TriggersOwner);
            move.Cond_TestReg(Reg_MulLeft, i, TriggerComparisonType::AtLeast);
            move.Action_DecReg(Reg_MulLeft, i);
            move.Action_IncReg(Reg_MulRight, i);
            m_Triggers.push_back(move.GetTrigger());
        }

        auto moveFinish = TriggerBuilder(moveAddress, nullptr, m_TriggersOwner);
        moveFinish.Cond_TestReg(Reg_MulLeft, 0, TriggerComparisonType::Exactly);
        moveFinish.Action_JumpTo(finishAddress);
        m_Triggers.push_back(moveFinish.GetTrigger());

        auto checkA = TriggerBuilder(checkAddress, nullptr, m_TriggersOwner);
        checkA.Cond_TestReg(Reg_Temp0, 0, TriggerComparisonType::Exactly);
        checkA.Cond_TestReg(Reg_Temp1, 0, TriggerComparisonType::Exactly);
        checkA.Action_JumpTo(moveAddress);
        m_Triggers.push_back(checkA.GetTrigger());

        auto checkB = TriggerBuilder(checkAddress, nullptr, m_TriggersOwner);
        checkB.Cond_TestReg(Reg_Temp0, 0, TriggerComparisonType::Exactly);
        checkB.Cond_TestReg(Reg_Temp1, 1, TriggerComparisonType::Exactly);
        checkB.Action_JumpTo(finishAddress);
        m_Triggers.push_back(checkB.GetTrigger());

        auto checkNotDoneA = TriggerBuilder(checkAddress, nullptr, m_TriggersOwner);
        checkNotDoneA.Cond_TestReg(Reg_Temp0, 1, TriggerComparisonType::AtLeast);
        checkNotDoneA.Cond_TestReg(Reg_Temp1, 0, TriggerComparisonType::Exactly);
        checkNotDoneA.Action_DecReg(Reg_Temp0, 1);
        checkNotDoneA.Action_SetReg(Reg_MulRight, 0);
        checkNotDoneA.Action_JumpTo(leftToRightAddress);
        m_Triggers.push_back(checkNotDoneA.GetTrigger());

        auto checkNotDoneB = TriggerBuilder(checkAddress, nullptr, m_TriggersOwner);
        checkNotDoneB.Cond_TestReg(Reg_Temp0, 1, TriggerComparisonType::AtLeast);
        checkNotDoneB.Cond_TestReg(Reg_Temp1, 1, TriggerComparisonType::Exactly);
        checkNotDoneB.Action_DecReg(Reg_Temp0, 1);
        checkNotDoneB.Action_SetReg(Reg_MulLeft, 0);
        checkNotDoneB.Action_JumpTo(rightToLeftAddress);
        m_Triggers.push_back(checkNotDoneB.GetTrigger());

        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto finish = TriggerBuilder(finishAddress, nullptr, m_TriggersOwner);
            finish.Cond_TestReg(Reg_Temp2, i, TriggerComparisonType::AtLeast);
            finish.Action_DecReg(Reg_Temp2, i);
            finish.Action_IncReg(Reg_MulRight, i);
            m_Triggers.push_back(finish.GetTrigger());
        }

        auto finishMul = TriggerBuilder(finishAddress, nullptr, m_TriggersOwner);
        finishMul.Cond_TestReg(Reg_Temp2, 0, TriggerComparisonType::Exactly);
        DoIndirectJump(finishMul);
        m_Triggers.push_back(finishMul.GetTrigger());
    }

}
