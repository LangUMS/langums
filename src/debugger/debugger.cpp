#include <iostream>
#include <chrono>

#include "../log.h"
#include "../compiler/compiler.h"

#include "memsearch.h"
#include "memwatch.h"
#include "reg_table.h"
#include "debugger.h"

#undef min
#undef max

#define VERSION "v0.1.0"

#define BASE_OFFSET 0x00e50000
#define GAME_SPEED_OFFSET 0x006509C4
#define SCENARIO_FILENAME "staredit\\scenario.chk"

namespace Langums
{

    Debugger::Debugger(const std::vector<std::unique_ptr<IIRInstruction>>& instructions, const std::string& source) : m_Source(source)
    {
        for (auto& instruction : instructions)
        {
            m_Instructions.push_back(instruction.get());
        }
    }

    bool Debugger::Attach(const std::string& processName)
    {
        if (m_DebuggerThread != nullptr)
        {
            return false;
        }

        m_DebuggerThread = std::make_unique<DebuggerThread>(processName);
        return true;
    }

    void Debugger::Detach()
    {
        m_DebuggerThread = nullptr;
    }
    
    DebuggerThreadState Debugger::GetState() const
    {
        if (m_DebuggerThread == nullptr)
        {
            return DebuggerThreadState::NotRunning;
        }

        return m_DebuggerThread->GetState();
    }

    unsigned int Debugger::GetCurrentAddress() const
    {
        return m_DebuggerThread->GetInstructionCounter();
    }

    IIRInstruction* Debugger::GetCurrentInstruction() const
    {
        return AddressToInstruction(GetCurrentAddress());
    }

    IASTNode* Debugger::GetCurrentASTNode() const
    {
        auto instruction = GetCurrentInstruction();
        if (instruction == nullptr)
        {
            return nullptr;
        }

        return instruction->GetASTNode();
    }

    int Debugger::GetCurrentSourceCharIndex() const
    {
        auto astNode = GetCurrentASTNode();
        if (astNode == nullptr)
        {
            return -1;
        }

        return astNode->GetCharIndex();
    }

    unsigned int Debugger::GetRegisterValue(unsigned int regId) const
    {
        return m_DebuggerThread->GetRegisterValue(regId);
    }

    void Debugger::SetRegisterValue(unsigned int regId, unsigned int value)
    {
        m_DebuggerThread->InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerSetRegisterCmd(regId, value)));
    }

    void Debugger::SuspendExecution()
    {
        m_DebuggerThread->InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerSuspendCmd()));
    }

    void Debugger::ResumeExecution()
    {
        m_DebuggerThread->InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerResumeCmd()));
    }

    void Debugger::SetBreakpoint(unsigned int breakpointId, unsigned int address)
    {
        m_DebuggerThread->InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerSetBreakpointCmd(breakpointId, address)));
    }

    void Debugger::RemoveBreakpoint(unsigned int breakpointId)
    {
        m_DebuggerThread->InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerRemoveBreakpointCmd(breakpointId)));
    }

    void Debugger::ContinueToNextAddress()
    {
        m_DebuggerThread->InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerContinueToNextCmd()));
    }

    int Debugger::GetCurrentBreakpointId() const
    {
        return m_DebuggerThread->GetCurrentBreakpoint();
    }

    IIRInstruction* Debugger::AddressToInstruction(unsigned int address) const
    {
        if (g_AddressToInstructionMap.find(address) == g_AddressToInstructionMap.end())
        {
            return nullptr;
        }

        return *g_AddressToInstructionMap[address].begin();
    }

}
