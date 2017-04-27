#include <cctype>

#include "../log.h"
#include "compiler.h"

namespace Langums
{

    bool Compiler::Compile(const std::vector<std::unique_ptr<IIRInstruction>>& instructions, CHK::File& chk, bool preserveTriggers)
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

        m_StackPointer = MAX_REGISTER_INDEX;

        for (auto i = 0u; i < instructions.size(); i++) // emit event triggers first
        {
            auto& instruction = instructions[i];
            auto type = instruction->GetType();
            
            if (type == IRInstructionType::Event)
            {
                auto evnt = (IREventInstruction*)instruction.get();
                auto conditionsCount = evnt->GetConditionsCount();
                if (conditionsCount == 0)
                {
                    throw CompilerException("Malformed input. Event with zero conditions");
                }

                TriggerBuilder eventTrigger(-1);
                eventTrigger.CodeGen_TestSwitch(Switch_EventsMutex, false);

                auto switchId = evnt->GetSwitchId();
                eventTrigger.CodeGen_SetSwitch(switchId, CHK::TriggerActionState::SetSwitch);

                for (auto q = 0u; q < conditionsCount; q++)
                {
                    i++;

                    auto& condition = instructions[i];
                    if (condition->GetType() == IRInstructionType::BringCond)
                    {
                        auto bring = (IRBringCondInstruction*)condition.get();

                        auto locationStringId = m_StringsChunk->FindString(bring->GetLocationName()) + 1;
                        auto locationId = m_LocationsChunk->FindLocation(locationStringId);
                        if (locationId == -1)
                        {
                            throw CompilerException(SafePrintf("Location \"%\" not found!", bring->GetLocationName()));
                        }

                        eventTrigger.CodeGen_Bring(
                            bring->GetPlayerId(),
                            (CHK::TriggerComparisonType)bring->GetComparison(),
                            bring->GetUnitId(),
                            locationId,
                            bring->GetQuantity()
                        );
                    }
                    else if (condition->GetType() == IRInstructionType::AccumCond)
                    {
                        auto accum = (IRAccumCondInstruction*)condition.get();

                        eventTrigger.CodeGen_Accumulate(
                            accum->GetPlayerId(),
                            (CHK::TriggerComparisonType)accum->GetComparison(),
                            accum->GetResourceType(),
                            accum->GetQuantity()
                        );
                    }
                    else if (condition->GetType() == IRInstructionType::TimeCond)
                    {
                        auto time = (IRTimeCondInstruction*)condition.get();

                        eventTrigger.CodeGen_ElapsedTime((CHK::TriggerComparisonType)time->GetComparison(), time->GetQuantity());
                    }
                    else if (condition->GetType() == IRInstructionType::CmdCond)
                    {
                        auto cmd = (IRCmdCondInstruction*)condition.get();
                        eventTrigger.CodeGen_Commands(cmd->GetPlayerId(), (CHK::TriggerComparisonType)cmd->GetComparison(), cmd->GetUnitId(), cmd->GetQuantity());
                    }
                    else if (condition->GetType() == IRInstructionType::KillCond)
                    {
                        auto kill = (IRKillCondInstruction*)condition.get();
                        eventTrigger.CodeGen_Kills(kill->GetPlayerId(), (CHK::TriggerComparisonType)kill->GetComparison(), kill->GetUnitId(), kill->GetQuantity());
                    }
                    else if (condition->GetType() == IRInstructionType::DeathCond)
                    {
                        auto death = (IRDeathCondInstruction*)condition.get();
                        eventTrigger.CodeGen_Deaths(death->GetPlayerId(), (CHK::TriggerComparisonType)death->GetComparison(), death->GetUnitId(), death->GetQuantity());
                    }
                    else if (condition->GetType() == IRInstructionType::CountdownCond)
                    {
                        auto countdown = (IRCountdownCondInstruction*)condition.get();
                        eventTrigger.CodeGen_Countdown((CHK::TriggerComparisonType)countdown->GetComparison(), countdown->GetTime());
                    }
                }

                m_Triggers.push_back(eventTrigger.GetTrigger());
            }
            else
            {
                break;
            }
        }

        // copy instruction counter from storage helper
        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            TriggerBuilder copyIC(-1);
            copyIC.CodeGen_TestSwitch(Switch_InstructionCounterMutex, true);
            copyIC.CodeGen_TestReg(Reg_CopyStorage, i, TriggerComparisonType::AtLeast);
            copyIC.CodeGen_DecReg(Reg_CopyStorage, i);
            copyIC.CodeGen_IncReg(Reg_InstructionCounter, i);
            m_Triggers.push_back(copyIC.GetTrigger());
        }

        TriggerBuilder copyICFinish(-1);
        copyICFinish.CodeGen_TestSwitch(Switch_InstructionCounterMutex, true);
        copyICFinish.CodeGen_TestReg(Reg_CopyStorage, 0, TriggerComparisonType::Exactly);
        copyICFinish.CodeGen_SetSwitch(Switch_InstructionCounterMutex, TriggerActionState::ClearSwitch);
        m_Triggers.push_back(copyICFinish.GetTrigger());
        //

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
            else if (type == IRInstructionType::Call)
            {
                auto call = (IRCallInstruction*)instruction.get();
                targetIndex = call->GetIndex();
            }
            
            if (targetIndex >= 0)
            {
                if (targetIndex >= (int)instructions.size())
                {
                    throw CompilerException(SafePrintf("Malformed IR. Trying to jump past last instruction (target address: %)", targetIndex));
                }

                m_JumpTargets.insert(instructions[targetIndex].get());
            }
        }

        std::unordered_map<CHK::Trigger*, IIRInstruction*> jmpPatchups;

        auto nextAddress = 0u;
        auto current = TriggerBuilder(nextAddress++);

        for (auto i = 0u; i < instructions.size(); i++)
        {
            auto& instruction = instructions[i];

            if (m_JumpTargets.find(instruction.get()) != m_JumpTargets.end())
            {
                if (current.HasChanges())
                {
                    auto address = nextAddress++;
                    m_JumpAddresses[instruction.get()] = address;

                    current.CodeGen_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());
                    current = TriggerBuilder(address, instruction.get());
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
                    current.CodeGen_SetReg(stackTop, push->GetRegisterId());
                }
                else
                {
                    auto retAddress = nextAddress++;
                    
                    auto stackTop = m_StackPointer--;
                    auto copyAddress = CodeGen_CopyReg(stackTop, push->GetRegisterId(), nextAddress, retAddress);

                    // clear storage and jump to step 1
                    current.CodeGen_SetReg(Reg_CopyStorage, 0);
                    current.CodeGen_JumpTo(copyAddress);
                    m_Triggers.push_back(current.GetTrigger());
                    current = TriggerBuilder(retAddress, instruction.get());
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

                    current.CodeGen_SetReg(regId, 0);
                    current.CodeGen_JumpTo(copyAddress);
                    m_Triggers.push_back(current.GetTrigger());

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto copy = TriggerBuilder(copyAddress);
                        copy.CodeGen_TestReg(stackTop, i, TriggerComparisonType::AtLeast);
                        copy.CodeGen_DecReg(stackTop, i);
                        copy.CodeGen_IncReg(regId, i);
                        m_Triggers.push_back(copy.GetTrigger());
                    }

                    current = TriggerBuilder(copyAddress);
                    current.CodeGen_TestReg(stackTop, 0, TriggerComparisonType::Exactly);
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

                current.CodeGen_SetReg(regId, setReg->GetValue());
            }
            else if (instruction->GetType() == IRInstructionType::IncReg)
            {
                auto incReg = (IRIncRegInstruction*)instruction.get();

                auto regId = incReg->GetRegisterId();
                if (regId >= Reg_StackTop)
                {
                    regId = m_StackPointer + (regId - Reg_StackTop) + 1;
                }

                current.CodeGen_IncReg(regId, incReg->GetAmount());
            }
            else if (instruction->GetType() == IRInstructionType::DecReg)
            {
                auto decReg = (IRDecRegInstruction*)instruction.get();
                auto regId = decReg->GetRegisterId();
                if (regId >= Reg_StackTop)
                {
                    regId = m_StackPointer + (regId - Reg_StackTop) + 1;
                }

                current.CodeGen_DecReg(regId, decReg->GetAmount());
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
                current.CodeGen_SetReg(Reg_CopyStorage, 0);
                current.CodeGen_JumpTo(copyAddress);

                m_Triggers.push_back(current.GetTrigger());
                current = TriggerBuilder(retAddress, instruction.get());
            }
            else if (instruction->GetType() == IRInstructionType::Add)
            {
                auto addAddress = nextAddress++;
                current.CodeGen_JumpTo(addAddress);

                m_Triggers.push_back(current.GetTrigger());
                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get());

                auto left = ++m_StackPointer;
                auto right = m_StackPointer + 1;

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto add = TriggerBuilder(addAddress, instruction.get());
                    add.CodeGen_TestReg(left, i, TriggerComparisonType::AtLeast);
                    add.CodeGen_DecReg(left, i);
                    add.CodeGen_IncReg(right, i);
                    m_Triggers.push_back(add.GetTrigger());
                }

                auto finishAdd = TriggerBuilder(addAddress, instruction.get());
                finishAdd.CodeGen_TestReg(left, 0, TriggerComparisonType::Exactly);
                finishAdd.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(finishAdd.GetTrigger());
            }
            else if (instruction->GetType() == IRInstructionType::Sub)
            {
                auto subAddress = nextAddress++;
                current.CodeGen_JumpTo(subAddress);

                m_Triggers.push_back(current.GetTrigger());
                auto retAddress = nextAddress++;
                current = TriggerBuilder(retAddress, instruction.get());

                auto left = ++m_StackPointer;
                auto right = m_StackPointer + 1;

                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto sub = TriggerBuilder(subAddress, instruction.get());
                    sub.CodeGen_TestReg(left, i, TriggerComparisonType::AtLeast);
                    sub.CodeGen_TestReg(right, i, TriggerComparisonType::AtLeast);
                    sub.CodeGen_DecReg(left, i);
                    sub.CodeGen_DecReg(right, i);
                    m_Triggers.push_back(sub.GetTrigger());
                }

                auto finishSub = TriggerBuilder(subAddress, instruction.get());
                finishSub.CodeGen_TestReg(left, 0, TriggerComparisonType::Exactly);
                finishSub.CodeGen_SetSwitch(Switch_ArithmeticUnderflow, TriggerActionState::ClearSwitch);
                finishSub.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(finishSub.GetTrigger());

                finishSub = TriggerBuilder(subAddress, instruction.get());
                finishSub.CodeGen_TestReg(left, 1, TriggerComparisonType::AtLeast);
                finishSub.CodeGen_TestReg(right, 0, TriggerComparisonType::Exactly);
                finishSub.CodeGen_SetSwitch(Switch_ArithmeticUnderflow, TriggerActionState::SetSwitch);
                finishSub.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(finishSub.GetTrigger());
            }
            else if (instruction->GetType() == IRInstructionType::Jmp)
            {
                auto jmp = (IRJmpInstruction*)instruction.get();
                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                auto targetInstruction = instructions[targetIndex].get();

                m_Triggers.push_back(current.GetTrigger());
                auto trigger = &m_Triggers.back();
                current = TriggerBuilder(nextAddress++, instruction.get());

                jmpPatchups.insert(std::make_pair(trigger, targetInstruction));
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfEqZero)
            {
                auto jmp = (IRJmpIfEqZeroInstruction*)instruction.get();

                auto regId = jmp->GetRegisterId();
                if (regId >= Reg_StackTop)
                {
                    regId = m_StackPointer + (regId - Reg_StackTop) + 1;
                }

                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();

                auto address = current.GetAddress();
                auto retAddress = nextAddress++;

                current.CodeGen_TestReg(regId, 1, TriggerComparisonType::AtLeast);
                current.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(current.GetTrigger());

                auto ifFalse = TriggerBuilder(address, instruction.get());
                ifFalse.CodeGen_TestReg(regId, 0, TriggerComparisonType::Exactly);
                m_Triggers.push_back(ifFalse.GetTrigger());
                auto trigger = &m_Triggers.back();
                jmpPatchups.insert(std::make_pair(trigger, instructions[targetIndex].get()));

                current = TriggerBuilder(retAddress, instruction.get());
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfNotEqZero)
            {
                auto jmp = (IRJmpIfNotEqZeroInstruction*)instruction.get();

                auto regId = jmp->GetRegisterId();
                if (regId >= Reg_StackTop)
                {
                    regId = m_StackPointer + (regId - Reg_StackTop) + 1;
                }

                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();

                auto address = current.GetAddress();
                auto retAddress = nextAddress++;

                current.CodeGen_TestReg(regId, 0, TriggerComparisonType::Exactly);
                current.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(current.GetTrigger());

                auto ifFalse = TriggerBuilder(address, instruction.get());
                ifFalse.CodeGen_TestReg(regId, 1, TriggerComparisonType::AtLeast);
                m_Triggers.push_back(ifFalse.GetTrigger());
                auto trigger = &m_Triggers.back();
                jmpPatchups.insert(std::make_pair(trigger, instructions[targetIndex].get()));

                current = TriggerBuilder(retAddress, instruction.get());
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfSwNotSet)
            {
                auto jmp = (IRJmpIfSwNotSetInstruction*)instruction.get();
                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                auto targetInstruction = instructions[targetIndex].get();;

                auto switchId = jmp->GetSwitchId();

                auto retAddress = nextAddress++;
                auto address = current.GetAddress();

                current.CodeGen_TestSwitch(switchId, true);
                current.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(current.GetTrigger());

                auto ifFalse = TriggerBuilder(address, instruction.get());
                ifFalse.CodeGen_TestSwitch(switchId, false);
                m_Triggers.push_back(ifFalse.GetTrigger());
                auto trigger = &m_Triggers.back();
                jmpPatchups.insert(std::make_pair(trigger, targetInstruction));

                current = TriggerBuilder(retAddress, instruction.get());
            }
            else if (instruction->GetType() == IRInstructionType::JmpIfSwSet)
            {
                auto jmp = (IRJmpIfSwSetInstruction*)instruction.get();
                auto targetIndex = jmp->IsAbsolute() ? jmp->GetOffset() : (int)i + jmp->GetOffset();
                auto targetInstruction = instructions[targetIndex].get();;

                auto switchId = jmp->GetSwitchId();

                auto retAddress = nextAddress++;
                auto address = current.GetAddress();

                current.CodeGen_TestSwitch(switchId, false);
                current.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(current.GetTrigger());

                auto ifFalse = TriggerBuilder(address, instruction.get());
                ifFalse.CodeGen_TestSwitch(switchId, true);
                m_Triggers.push_back(ifFalse.GetTrigger());
                auto trigger = &m_Triggers.back();
                jmpPatchups.insert(std::make_pair(trigger, targetInstruction));

                current = TriggerBuilder(retAddress, instruction.get());
            }
            else if (instruction->GetType() == IRInstructionType::SetSw)
            {
                auto setSwitch = (IRSetSwInstruction*)instruction.get();
                current.CodeGen_SetSwitch(setSwitch->GetSwitchId(), setSwitch->GetState() ? TriggerActionState::SetSwitch : TriggerActionState::ClearSwitch);
            }
            else if (instruction->GetType() == IRInstructionType::DisplayMsg)
            {
                auto address = nextAddress++;
                current.CodeGen_JumpTo(address);
                m_Triggers.push_back(current.GetTrigger());

                auto displayMsgReg = (IRDisplayMsgInstruction*)instruction.get();
                auto playerMask = displayMsgReg->GetPlayerMask();
                auto msgTrigger = TriggerBuilder(address, instruction.get(), playerMask);

                auto stringId = m_StringsChunk->InsertString(displayMsgReg->GetMessage());
                msgTrigger.CodeGen_DisplayMsg(stringId);

                auto retAddress = nextAddress++;
                msgTrigger.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(msgTrigger.GetTrigger());

                current = TriggerBuilder(retAddress, instruction.get());
            }
            else if (instruction->GetType() == IRInstructionType::Wait)
            {
                auto wait = (IRWaitInstruction*)instruction.get();
                current.CodeGen_Wait(wait->GetMilliseconds());
            }
            else if (instruction->GetType() == IRInstructionType::Spawn)
            {
                auto spawn = (IRSpawnInstruction*)instruction.get();

                auto locationStringId = m_StringsChunk->FindString(spawn->GetLocationName()) + 1;
                auto locationId = m_LocationsChunk->FindLocation(locationStringId);
                if (locationId == -1)
                {
                    throw CompilerException(SafePrintf("Location \"%\" not found!", spawn->GetLocationName()));
                }
                
                if (spawn->IsValueLiteral())
                {
                    current.CodeGen_CreateUnit(spawn->GetPlayerId(), spawn->GetUnitId(), spawn->GetRegisterId(), locationId);
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
                    current.CodeGen_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get());

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto spawnTrigger = TriggerBuilder(address, instruction.get());
                        spawnTrigger.CodeGen_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        spawnTrigger.CodeGen_DecReg(regId, i);
                        spawnTrigger.CodeGen_CreateUnit(spawn->GetPlayerId(), spawn->GetUnitId(), i, locationId);
                        m_Triggers.push_back(spawnTrigger.GetTrigger());
                    }

                    auto finishSpawn = TriggerBuilder(address, instruction.get());
                    finishSpawn.CodeGen_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishSpawn.CodeGen_JumpTo(retAddress);
                    m_Triggers.push_back(finishSpawn.GetTrigger());
                }
            }
            else if (instruction->GetType() == IRInstructionType::Kill)
            {
                auto kill = (IRKillInstruction*)instruction.get();

                auto locationId = -1;
                auto locName = kill->GetLocationName();
                if (locName.length() != 0)
                {
                    auto locationStringId = m_StringsChunk->FindString(kill->GetLocationName()) + 1;
                    auto locationId = m_LocationsChunk->FindLocation(locationStringId);
                    if (locationId == -1)
                    {
                        throw CompilerException(SafePrintf("Location \"%\" not found!", kill->GetLocationName()));
                    }
                }

                if (kill->IsValueLiteral())
                {
                    current.CodeGen_KillUnit(kill->GetPlayerId(), kill->GetUnitId(), kill->GetRegisterId(), locationId);
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
                    current.CodeGen_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get());

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto killTrigger = TriggerBuilder(address, instruction.get());
                        killTrigger.CodeGen_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        killTrigger.CodeGen_DecReg(regId, i);
                        killTrigger.CodeGen_KillUnit(kill->GetPlayerId(), kill->GetUnitId(), i, locationId);
                        m_Triggers.push_back(killTrigger.GetTrigger());
                    }

                    auto finishTrigger = TriggerBuilder(address, instruction.get());
                    finishTrigger.CodeGen_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishTrigger.CodeGen_JumpTo(retAddress);
                    m_Triggers.push_back(finishTrigger.GetTrigger());
                }
            }
            else if (instruction->GetType() == IRInstructionType::Move)
            {
                auto move = (IRMoveInstruction*)instruction.get();

                auto srcLocName = move->GetSrcLocationName();
                auto locationStringId = m_StringsChunk->FindString(srcLocName) + 1;
                auto srcLocationId = m_LocationsChunk->FindLocation(locationStringId);

                if (srcLocationId == -1)
                {
                    throw CompilerException(SafePrintf("Location \"%\" not found!", srcLocName));
                }

                auto dstLocName = move->GetDstLocationName();
                locationStringId = m_StringsChunk->FindString(dstLocName) + 1;
                auto dstLocationId = m_LocationsChunk->FindLocation(locationStringId);

                if (dstLocationId == -1)
                {
                    throw CompilerException(SafePrintf("Location \"%\" not found!", dstLocName));
                }

                if (move->IsValueLiteral())
                {
                    current.CodeGen_MoveUnit(move->GetPlayerId(), move->GetUnitId(), move->GetRegisterId(), srcLocationId, dstLocationId);
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
                    current.CodeGen_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get());

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto moveTrigger = TriggerBuilder(address, instruction.get());
                        moveTrigger.CodeGen_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        moveTrigger.CodeGen_DecReg(regId, i);
                        current.CodeGen_MoveUnit(move->GetPlayerId(), move->GetUnitId(), i, srcLocationId, dstLocationId);
                        m_Triggers.push_back(moveTrigger.GetTrigger());
                    }

                    auto finishTrigger = TriggerBuilder(address, instruction.get());
                    finishTrigger.CodeGen_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishTrigger.CodeGen_JumpTo(retAddress);
                    m_Triggers.push_back(finishTrigger.GetTrigger());
                }
            }
            else if (instruction->GetType() == IRInstructionType::MoveLoc)
            {
                auto move = (IRMoveLocInstruction*)instruction.get();

                auto srcLocName = move->GetSrcLocationName();
                auto locationStringId = m_StringsChunk->FindString(srcLocName) + 1;
                auto srcLocationId = m_LocationsChunk->FindLocation(locationStringId);

                if (srcLocationId == -1)
                {
                    throw CompilerException(SafePrintf("Location \"%\" not found!", srcLocName));
                }

                auto dstLocName = move->GetDstLocationName();
                locationStringId = m_StringsChunk->FindString(dstLocName) + 1;
                auto dstLocationId = m_LocationsChunk->FindLocation(locationStringId);

                if (dstLocationId == -1)
                {
                    throw CompilerException(SafePrintf("Location \"%\" not found!", dstLocName));
                }

                current.CodeGen_MoveLocation(move->GetPlayerId(), move->GetUnitId(), srcLocationId, dstLocationId);
            }
            else if (instruction->GetType() == IRInstructionType::EndGame)
            {
                auto endGame = (IREndGameInstruction*)instruction.get();

                auto type = endGame->GetEndGameType();
                auto playerMask = endGame->GetPlayerMask();

                auto address = nextAddress++;
                current.CodeGen_JumpTo(address);
                m_Triggers.push_back(current.GetTrigger());

                auto endGameTrigger = TriggerBuilder(address, instruction.get(), playerMask);

                if (type == EndGameType::Victory)
                {
                    endGameTrigger.CodeGen_Victory();
                }
                else if (type == EndGameType::Defeat)
                {
                    endGameTrigger.CodeGen_Defeat();
                }
                else if (type == EndGameType::Draw)
                {
                    endGameTrigger.CodeGen_Draw();
                }

                auto retAddress = nextAddress++;
                endGameTrigger.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(endGameTrigger.GetTrigger());

                current = TriggerBuilder(retAddress, instruction.get());
            }
            else if (instruction->GetType() == IRInstructionType::CenterView)
            {
                auto centerView = (IRCenterViewInstruction*)instruction.get();
                auto playerMask = centerView->GetPlayerId();

                auto locName = centerView->GetLocationName();
                if (locName.length() == 0)
                {
                    throw CompilerException(SafePrintf("Empty location \"%\" in center view instruction!", centerView->GetLocationName()));
                }

                auto locationStringId = m_StringsChunk->FindString(centerView->GetLocationName()) + 1;
                auto locationId = m_LocationsChunk->FindLocation(locationStringId);
                if (locationId == -1)
                {
                    throw CompilerException(SafePrintf("Location \"%\" not found!", centerView->GetLocationName()));
                }

                auto address = nextAddress++;
                current.CodeGen_JumpTo(address);
                m_Triggers.push_back(current.GetTrigger());

                auto centerViewTrigger = TriggerBuilder(address, instruction.get(), playerMask + 1);

                centerViewTrigger.CodeGen_CenterView(locationId + 1);

                auto retAddress = nextAddress++;
                centerViewTrigger.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(centerViewTrigger.GetTrigger());

                current = TriggerBuilder(retAddress, instruction.get());
            }
            else if (instruction->GetType() == IRInstructionType::Ping)
            {
                auto ping = (IRPingInstruction*)instruction.get();
                auto playerMask = ping->GetPlayerId();

                auto locName = ping->GetLocationName();
                if (locName.length() == 0)
                {
                    throw CompilerException(SafePrintf("Empty location \"%\" in ping instruction!", ping->GetLocationName()));
                }

                auto locationStringId = m_StringsChunk->FindString(ping->GetLocationName()) + 1;
                auto locationId = m_LocationsChunk->FindLocation(locationStringId);
                if (locationId == -1)
                {
                    throw CompilerException(SafePrintf("Location \"%\" not found!", ping->GetLocationName()));
                }

                auto address = nextAddress++;
                current.CodeGen_JumpTo(address);
                m_Triggers.push_back(current.GetTrigger());

                auto pingTrigger = TriggerBuilder(address, instruction.get(), playerMask + 1);

                pingTrigger.CodeGen_Ping(locationId + 1);

                auto retAddress = nextAddress++;
                pingTrigger.CodeGen_JumpTo(retAddress);
                m_Triggers.push_back(pingTrigger.GetTrigger());

                current = TriggerBuilder(retAddress, instruction.get());
            }
            else if (instruction->GetType() == IRInstructionType::SetResource)
            {
                auto setResource = (IRSetResourceInstruction*)instruction.get();
                auto playerId = setResource->GetPlayerId();

                if (setResource->IsValueLiteral())
                {
                    auto quantity = setResource->GetRegisterId();
                    current.CodeGen_SetResources(playerId, quantity, TriggerActionState::SetTo, setResource->GetResourceType());
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
                    current.CodeGen_SetResources(playerId, 0, TriggerActionState::SetTo, setResource->GetResourceType());
                    current.CodeGen_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get());

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get());
                        add.CodeGen_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.CodeGen_DecReg(regId, i);
                        add.CodeGen_SetResources(playerId, i, TriggerActionState::Add, setResource->GetResourceType());
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get());
                    finishAdd.CodeGen_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.CodeGen_JumpTo(retAddress);
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
                    current.CodeGen_SetResources(playerId, quantity, TriggerActionState::Add, setResource->GetResourceType());
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
                    current.CodeGen_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get());

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get());
                        add.CodeGen_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.CodeGen_DecReg(regId, i);
                        add.CodeGen_SetResources(playerId, i, TriggerActionState::Add, setResource->GetResourceType());
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get());
                    finishAdd.CodeGen_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.CodeGen_JumpTo(retAddress);
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
                    current.CodeGen_SetResources(playerId, quantity, TriggerActionState::Subtract, setResource->GetResourceType());
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
                    current.CodeGen_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get());

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto sub = TriggerBuilder(address, instruction.get());
                        sub.CodeGen_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        sub.CodeGen_DecReg(regId, i);
                        sub.CodeGen_SetResources(playerId, i, TriggerActionState::Subtract, setResource->GetResourceType());
                        m_Triggers.push_back(sub.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get());
                    finishAdd.CodeGen_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.CodeGen_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
                }
            }
            else if (instruction->GetType() == IRInstructionType::SetCountdown)
            {
                auto setCountdown = (IRSetCountdownInstruction*)instruction.get();

                if (setCountdown->IsValueLiteral())
                {
                    auto time = setCountdown->GetRegisterId();
                    current.CodeGen_SetCountdown(time, TriggerActionState::SetTo);
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
                    current.CodeGen_SetCountdown(0, TriggerActionState::SetTo);
                    current.CodeGen_JumpTo(address);
                    m_Triggers.push_back(current.GetTrigger());

                    auto retAddress = nextAddress++;
                    current = TriggerBuilder(retAddress, instruction.get());

                    for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                    {
                        auto add = TriggerBuilder(address, instruction.get());
                        add.CodeGen_TestReg(regId, i, TriggerComparisonType::AtLeast);
                        add.CodeGen_DecReg(regId, i);
                        add.CodeGen_SetCountdown(i, TriggerActionState::Add);
                        m_Triggers.push_back(add.GetTrigger());
                    }

                    auto finishAdd = TriggerBuilder(address, instruction.get());
                    finishAdd.CodeGen_TestReg(regId, 0, TriggerComparisonType::Exactly);
                    finishAdd.CodeGen_JumpTo(retAddress);
                    m_Triggers.push_back(finishAdd.GetTrigger());
                }
            }
            else if (instruction->GetType() == IRInstructionType::Call)
            {
                auto call = (IRCallInstruction*)instruction.get();
                
                auto retAddress = nextAddress++;
                current.CodeGen_SetReg(call->GetReturnRegisterId(), retAddress);
                m_Triggers.push_back(current.GetTrigger());
                auto trigger = &m_Triggers.back();

                auto targetIndex = call->GetIndex();
                jmpPatchups.insert(std::make_pair(trigger, instructions[targetIndex].get()));

                current = TriggerBuilder(retAddress, instruction.get());
            }
            else if (instruction->GetType() == IRInstructionType::Ret)
            {
                auto address = nextAddress++;
                current.CodeGen_SetReg(Reg_CopyStorage, 0);
                current.CodeGen_JumpTo(address);
                m_Triggers.push_back(current.GetTrigger());

                auto ret = (IRRetInstruction*)instruction.get();
                auto regId = ret->GetRegisterId();

                // step 1 - copy to storage
                for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
                {
                    auto copyToStorageTrigger = TriggerBuilder(address);
                    copyToStorageTrigger.CodeGen_TestReg(regId, i, TriggerComparisonType::AtLeast);
                    copyToStorageTrigger.CodeGen_DecReg(regId, i);
                    copyToStorageTrigger.CodeGen_IncReg(Reg_CopyStorage, i);
                    m_Triggers.push_back(copyToStorageTrigger.GetTrigger());
                }

                // step 1 (finish)
                auto finishCopyTrigger = TriggerBuilder(address);
                finishCopyTrigger.CodeGen_TestReg(regId, 0, TriggerComparisonType::Exactly);
                finishCopyTrigger.CodeGen_SetReg(Reg_InstructionCounter, 0);
                finishCopyTrigger.CodeGen_SetSwitch(Switch_InstructionCounterMutex, TriggerActionState::SetSwitch);
                m_Triggers.push_back(finishCopyTrigger.GetTrigger());
            }
            else if
            (
                instruction->GetType() == IRInstructionType::Event ||
                instruction->GetType() == IRInstructionType::BringCond ||
                instruction->GetType() == IRInstructionType::AccumCond ||
                instruction->GetType() == IRInstructionType::TimeCond ||
                instruction->GetType() == IRInstructionType::KillCond ||
                instruction->GetType() == IRInstructionType::DeathCond ||
                instruction->GetType() == IRInstructionType::CmdCond ||
                instruction->GetType() == IRInstructionType::CountdownCond
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

            CodeGen_JumpTo(targetAddress, trigger.m_Actions[actionId]);
        }

        for (auto q = 0u; q < m_HyperTriggerCount; q++)
        {
            Trigger hyperTrigger;
            hyperTrigger.m_ExecutionFlags = 0;
            hyperTrigger.m_ExecutionMask[7] = 1;

            CodeGen_Always(hyperTrigger.m_Conditions[0]);
            CodeGen_PreserveTrigger(hyperTrigger.m_Actions[0]);
            
            for (auto i = 1; i < 64; i++)
            {
                CodeGen_Wait(0, hyperTrigger.m_Actions[i]);
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

    void Compiler::CodeGen_Always(CHK::TriggerCondition& retCondition)
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
            auto copyToStorageTrigger = TriggerBuilder(copyAddress);
            copyToStorageTrigger.CodeGen_TestReg(srcReg, i, TriggerComparisonType::AtLeast);
            copyToStorageTrigger.CodeGen_DecReg(srcReg, i);
            copyToStorageTrigger.CodeGen_IncReg(Reg_CopyStorage, i);
            m_Triggers.push_back(copyToStorageTrigger.GetTrigger());
        }

        // step 1 (finish) - finish copy and jump to step 2
        auto finishCopyTrigger = TriggerBuilder(copyAddress);
        finishCopyTrigger.CodeGen_TestReg(srcReg, 0, TriggerComparisonType::Exactly);
        finishCopyTrigger.CodeGen_SetReg(dstReg, 0);
        finishCopyTrigger.CodeGen_JumpTo(copy2Address);
        m_Triggers.push_back(finishCopyTrigger.GetTrigger());

        // step 3 - copy from storage
        for (auto i = m_CopyBatchSize; i >= 1; i /= 2)
        {
            auto copyFromStorageTrigger = TriggerBuilder(copy2Address);
            copyFromStorageTrigger.CodeGen_TestReg(Reg_CopyStorage, i, TriggerComparisonType::AtLeast);
            copyFromStorageTrigger.CodeGen_DecReg(Reg_CopyStorage, i);
            copyFromStorageTrigger.CodeGen_IncReg(srcReg, i);
            copyFromStorageTrigger.CodeGen_IncReg(dstReg, i);
            m_Triggers.push_back(copyFromStorageTrigger.GetTrigger());
        }

        // step 3 (finish)
        auto finishCopyFromStorageTrigger = TriggerBuilder(copy2Address);
        finishCopyFromStorageTrigger.CodeGen_TestReg(Reg_CopyStorage, 0, TriggerComparisonType::Exactly);
        finishCopyFromStorageTrigger.CodeGen_JumpTo(retAddress);
        m_Triggers.push_back(finishCopyFromStorageTrigger.GetTrigger());

        return copyAddress;
    }

    void Compiler::CodeGen_PreserveTrigger(CHK::TriggerAction& retAction)
    {
        using namespace CHK;
        retAction.m_ActionType = TriggerActionType::PreserveTrigger;
    }

    void Compiler::CodeGen_Wait(unsigned int milliseconds, CHK::TriggerAction& retAction)
    {
        using namespace CHK;
        retAction.m_Group = 7;
        retAction.m_ActionType = TriggerActionType::Wait;
        retAction.m_Milliseconds = milliseconds;
        retAction.m_Flags = 16;
    }

    void Compiler::CodeGen_JumpTo(unsigned int address, CHK::TriggerAction& retAction)
    {
        using namespace CHK;
        retAction.m_ActionType = TriggerActionType::SetDeaths;
        retAction.m_Group = 7;
        retAction.m_Modifier = (uint8_t)TriggerActionState::SetTo;
        retAction.m_Flags = 16;
        retAction.m_Arg0 = address;
        retAction.m_Arg1 = Reg_InstructionCounter;
    }

    int Compiler::GetLastTriggerActionId(const CHK::Trigger& trigger)
    {
        auto index = 0;

        while (trigger.m_Actions[index].m_ActionType != CHK::TriggerActionType::NoAction)
        {
            index++;
            if (index >= 64)
            {
                return -1;
            }
        }

        return index;
    }

}
