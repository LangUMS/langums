#ifndef __LANGUMS_DEBUGGER_THREAD_H
#define __LANGUMS_DEBUGGER_THREAD_H

#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>

#include "reg_table.h"
#include "debugger_cmds.h"

namespace Langums
{

    enum class DebuggerThreadState
    {
        NotRunning = 0,
        WaitingForProcess,
        Attached,
        Suspended,
        WaitingAtBreakpoint
    };

    struct DebuggerBreakpoint
    {
        unsigned int m_Address;
        bool m_Enabled;
    };

    class DebuggerThread
    {
        public:
        DebuggerThread(const std::string& processName);
        ~DebuggerThread();

        const std::string& GetProcessName() const
        {
            return m_ProcessName;
        }

        unsigned int GetRegisterValue(unsigned int regId);

        unsigned int GetInstructionCounter() const
        {
            return m_InstructionCounter;
        }

        unsigned int GetCurrentBreakpoint() const
        {
            return m_CurrentBreakpoint;
        }

        DebuggerThreadState GetState() const
        {
            return m_State;
        }

        void InsertCommand(std::unique_ptr<IDebuggerCmd> cmd)
        {
            std::unique_lock<std::mutex> _(m_CommandQueueMutex);
            m_CommandQueue.push_back(std::move(cmd));
        }

        private:
        void Run();
        void ProcessCommand(std::unique_ptr<IDebuggerCmd> cmd);
        IIRInstruction* AddressToInstruction(unsigned int address) const;

        std::atomic<DebuggerThreadState> m_State = DebuggerThreadState::NotRunning;

        std::deque<std::unique_ptr<IDebuggerCmd>> m_CommandQueue;
        std::mutex m_CommandQueueMutex;

        std::unique_ptr<RegTable> m_Registers;
        std::mutex m_RegistersMutex;

        std::unique_ptr<std::thread> m_Thread = nullptr;

        std::string m_ProcessName;
        std::atomic<bool> m_ThreadShutdown;

        std::atomic<unsigned int> m_InstructionCounter;
        Process::ProcessHandle m_Process;
        void* m_BaseAddress = nullptr;

        unsigned int m_PollRate = 240; // poll the game for changes N times per second

        std::unordered_map<unsigned int, DebuggerBreakpoint> m_Breakpoints;
        std::atomic<unsigned int> m_CurrentBreakpoint = 0;

        int m_SuspendOnAddress = -1;
        int m_LastBreakAddress = -1;
    };

}

#endif
