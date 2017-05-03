#ifndef __LANGUMS_DEBUGGER_H
#define __LANGUMS_DEBUGGER_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>

#include "process_util.h"
#include "reg_table.h"

namespace Langums
{

    enum class DebuggerState
    {
        NotRunning = 0,
        WaitingForProcess,
        Attached,
        WaitingAtBreakpoint
    };

    class Debugger
    {
        public:
        Debugger(const std::vector<std::unique_ptr<IIRInstruction>>& instructions, const std::string& source);
        ~Debugger();

        DebuggerState GetState() const
        {
            return m_State;
        }

        bool Attach(const std::string& processName);
        void Detach();

        unsigned int GetCurrentAddress() const
        {
            return m_InstructionCounter;
        }

        IIRInstruction* GetCurrentInstruction() const
        {
            return AddressToInstruction(m_InstructionCounter);
        }

        IASTNode* GetCurrentASTNode() const
        {
            auto instruction = GetCurrentInstruction();
            if (instruction == nullptr)
            {
                return nullptr;
            }

            return instruction->GetASTNode();
        }

        int GetCurrentSourceCharIndex() const
        {
            auto astNode = GetCurrentASTNode();
            if (astNode == nullptr)
            {
                return -1;
            }

            return astNode->GetCharIndex();
        }

        private:
        void PauseGame(bool state);
        void RunThread();
        IIRInstruction* AddressToInstruction(unsigned int address) const;

        DebuggerState m_State = DebuggerState::NotRunning;
        std::unique_ptr<std::thread> m_Thread = nullptr;

        // read only
        std::vector<IIRInstruction*> m_Instructions;
        std::string m_Source;
        unsigned int m_PollRate = 240; // poll the game for changes N times per second
        std::string m_ProcessName;

        // stuff below belongs to the debugger thread
        std::atomic<unsigned int> m_InstructionCounter;
        std::atomic<bool> m_ShutdownThread = false;
        std::unique_ptr<RegTable> m_Registers;
        Process::ProcessHandle m_Process;
        void* m_BaseAddress = nullptr;
    };

}

#endif
