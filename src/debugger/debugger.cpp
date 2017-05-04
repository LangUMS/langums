#include <iostream>
#include <chrono>

#include "../log.h"
#include "../compiler/compiler.h"

#include "memsearch.h"
#include "memwatch.h"
#include "reg_table.h"
#include "debugger.h"
#include "../pretty_errors.h"

#undef min
#undef max

#define VERSION "v0.1.0"

#define BASE_OFFSET 0x00e50000
#define GAME_SPEED_OFFSET 0x006509C4
#define SCENARIO_FILENAME "staredit\\scenario.chk"

namespace Langums
{

    Debugger::Debugger(const std::vector<std::unique_ptr<IIRInstruction>>& instructions, const std::string& source, const std::shared_ptr<IASTNode>& ast) :
        m_Source(source)
    {
        m_AST = ast.get();
        m_SourceLines = Split(source);

        auto i = 0u;
        for (auto& line : m_SourceLines)
        {
            m_LineStartIndices.push_back(i);
            i += line.length() + 1;
        }

        for (auto& instruction : instructions)
        {
            m_Instructions.push_back(instruction.get());
        }

        GatherASTNodeCharIndices(m_AST);
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
        m_BreakpointIdToAddress[breakpointId] = address;

        auto id = m_NextBreakpointId++;
        m_Breakpoints[breakpointId].push_back(id);
        m_DebuggerThread->InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerSetBreakpointCmd(id, address)));
    }

    bool Debugger::SetBreakpointBySourceLine(unsigned int breakpointId, unsigned int lineNumber)
    {
        auto instructions = SourceLineToInstructions(lineNumber);
        if (instructions.size() == 0)
        {
            return false;
        }

        auto set = false;

        for (auto instruction : instructions)
        {
            auto address = InstructionToAddress(instruction);
            if (address != -1)
            {
                auto exists = false;

                for (auto& pair : m_BreakpointIdToAddress)
                {
                    if (pair.second == address)
                    {
                        exists = true;
                        break;
                    }
                }

                if (!exists)
                {
                    SetBreakpoint(breakpointId, address);
                    set = true;
                }
            }
        }
        
        return set;
    }

    void Debugger::RemoveBreakpoint(unsigned int breakpointId)
    {
        for (auto& id : m_Breakpoints[breakpointId])
        {
            m_BreakpointIdToAddress.erase(id);

            m_DebuggerThread->InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerRemoveBreakpointCmd(id)));
        }

        m_Breakpoints.erase(breakpointId);
    }

    void Debugger::RemoveAllBreakpoints()
    {
        for (auto& pair : m_Breakpoints)
        {
            for (auto& id : pair.second)
            {
                m_DebuggerThread->InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerRemoveBreakpointCmd(id)));
            }
        }

        m_Breakpoints.clear();
        m_BreakpointIdToAddress.clear();
    }

    void Debugger::ContinueToNextAddress()
    {
        m_DebuggerThread->InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerContinueToNextCmd()));
    }

    int Debugger::GetCurrentBreakpointId() const
    {
        return m_DebuggerThread->GetCurrentBreakpoint();
    }

    void Debugger::GatherASTNodeCharIndices(IASTNode* node)
    {
        m_ASTNodeCharIndices.insert(std::make_pair(node, node->GetCharIndex()));

        auto& children = node->GetChildren();
        for (auto& child : children)
        {
            GatherASTNodeCharIndices(child.get());
        }
    }

    IIRInstruction* Debugger::AddressToInstruction(unsigned int address) const
    {
        if (g_AddressToInstructionMap.find(address) == g_AddressToInstructionMap.end())
        {
            return nullptr;
        }

        return *g_AddressToInstructionMap[address].begin();
    }

    std::vector<IIRInstruction*> Debugger::SourceLineToInstructions(unsigned int lineNumber)
    {
        auto startIndex = m_LineStartIndices[lineNumber - 1];
        
        std::vector<IASTNode*> nodes;
        int closestDistance = std::numeric_limits<int>::max();

        for (auto& pair : m_ASTNodeCharIndices)
        {
            if (pair.second < startIndex)
            {
                continue;
            }

            auto dist = pair.second - startIndex;
            if (dist < closestDistance)
            {
                closestDistance = pair.second - startIndex;
                nodes.clear();
                nodes.push_back(pair.first);
            }
            else if (dist == closestDistance)
            {
                nodes.push_back(pair.first);
            }
        }

        std::set<IIRInstruction*> instructions;

        for (auto node : nodes)
        {
            while (node != nullptr)
            {
                auto stop = false;

                for (auto& instruction : m_Instructions)
                {
                    if (instruction->GetASTNode() == node)
                    {
                        instructions.insert(instruction);
                        stop = true;
                        break;
                    }
                }

                if (stop)
                {
                    break;
                }

                node = node->GetParent();
            }
        }

        std::vector<IIRInstruction*> ret;
        for (auto& item : instructions)
        {
            ret.push_back(item);
        }

        return ret;
    }

    int Debugger::InstructionToAddress(IIRInstruction* instruction)
    {
        for (auto& pair : g_AddressToInstructionMap)
        {
            if (pair.second.count(instruction) > 0)
            {
                return pair.first;
            }
        }

        return -1;
    }

}
