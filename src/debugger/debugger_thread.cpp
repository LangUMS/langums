#include <chrono>

#include "../log.h"

#include "../compiler/compiler.h"
#include "debugger_thread.h"

namespace Langums
{

    DebuggerThread::DebuggerThread(const std::string& processName) : m_ProcessName(processName)
    {
        m_Thread = std::make_unique<std::thread>(std::bind(&DebuggerThread::Run, this));
    }

    DebuggerThread::~DebuggerThread()
    {
        if (m_Process != nullptr)
        {
            Process::Resume(m_Process);
            Process::Close(m_Process);
        }

        if (!m_ThreadShutdown)
        {
            InsertCommand(std::unique_ptr<IDebuggerCmd>(new DebuggerShutdownCmd()));

            while (!m_ThreadShutdown)
            {
            }
        }

        m_Thread->join();
    }

    void DebuggerThread::Run()
    {
        using namespace std::chrono;

        LOG_F("Debugger: Looking for \"%\"", m_ProcessName);

        auto before = high_resolution_clock::now();

        m_State = DebuggerThreadState::WaitingForProcess;

        m_Process = nullptr;
        while (m_Process == nullptr)
        {
            m_Process = Process::OpenByName(m_ProcessName, m_BaseAddress);
            std::this_thread::sleep_for(milliseconds(500));

            auto now = high_resolution_clock::now();

            if ((now - before) > seconds(90))
            {
                LOG_F("\n(!) Debugger: Failed to find \"%\" for 90 seconds, bailing out...", m_ProcessName);
                m_State = DebuggerThreadState::NotRunning;
                m_ThreadShutdown = true;
                return;
            }
        }

        m_State = DebuggerThreadState::Attached;

        m_Registers = std::make_unique<RegTable>(m_Process, m_BaseAddress);
        LOG_F("Debugger: Attached to process.");

        auto updateRate = 1.0 / (double)m_PollRate;
        before = high_resolution_clock::now();
        auto acc = 0.0;

        while (true)
        {
            std::unique_ptr<IDebuggerCmd> cmd = nullptr;

            {
                std::unique_lock<std::mutex> _(m_CommandQueueMutex);
                if (!m_CommandQueue.empty())
                {
                    cmd = std::move(m_CommandQueue.front());
                    m_CommandQueue.pop_front();
                }
            }

            if (cmd != nullptr)
            {
                if (cmd->GetType() == DebuggerCmdType::Shutdown)
                {
                    m_ThreadShutdown = true;
                    return;
                }
                else
                {
                    ProcessCommand(std::move(cmd));
                }
            }

            auto now = high_resolution_clock::now();
            auto dt = (double)duration_cast<milliseconds>(now - before).count() / 1000.0;
            before = now;

            acc += dt;
            if (acc >= updateRate)
            {
                if (!Process::IsRunning(m_Process))
                {
                    m_State = DebuggerThreadState::NotRunning;
                    m_ThreadShutdown = true;
                    return;
                }

                {
                    auto oldInstructionCounter = (unsigned int)m_InstructionCounter;

                    std::unique_lock<std::mutex> _(m_RegistersMutex);
                    m_Registers->Update();

                    auto& icDef = g_RegisterMap[Reg_InstructionCounter];
                    m_InstructionCounter = m_Registers->Get(icDef.m_PlayerId, icDef.m_Index);

                    auto instruction = AddressToInstruction(m_InstructionCounter);
                    if (m_InstructionCounter != m_LastBreakAddress && instruction && instruction->GetType() == IRInstructionType::DebugBrk)
                    {
                        m_LastBreakAddress = m_InstructionCounter;
                        Process::Suspend(m_Process);
                        m_State = DebuggerThreadState::WaitingAtBreakpoint;
                        continue;
                    }

                    if (m_SuspendOnAddress != -1 && m_SuspendOnAddress != m_InstructionCounter)
                    {
                        Process::Suspend(m_Process);
                        m_SuspendOnAddress = -1;
                        m_LastBreakAddress = m_InstructionCounter;
                        m_State = DebuggerThreadState::WaitingAtBreakpoint;
                        continue;
                    }
                }

                for (auto& pair : m_Breakpoints)
                {
                    auto& breakpoint = pair.second;
                    if (breakpoint.m_Address == m_InstructionCounter)
                    {
                        Process::Suspend(m_Process);
                        m_CurrentBreakpoint = pair.first;
                        m_State = DebuggerThreadState::WaitingAtBreakpoint;
                    }
                }

                acc -= updateRate;
            }

            if (dt < updateRate)
            {
                std::this_thread::sleep_for(milliseconds((size_t)((updateRate - dt) * 1000.0)));
            }
        }
    }

    unsigned int DebuggerThread::GetRegisterValue(unsigned int regId)
    {
        std::unique_lock<std::mutex> _(m_RegistersMutex);
        auto& regDef = g_RegisterMap[regId];
        return m_Registers->Get(regDef.m_PlayerId, regDef.m_Index);
    }

    void DebuggerThread::ProcessCommand(std::unique_ptr<IDebuggerCmd> cmd)
    {
        if (cmd->GetType() == DebuggerCmdType::Suspend)
        {
            Process::Suspend(m_Process);
            m_State = DebuggerThreadState::Suspended;
        }
        else if (cmd->GetType() == DebuggerCmdType::Resume)
        {
            Process::Resume(m_Process);
            m_State = DebuggerThreadState::Attached;
            m_CurrentBreakpoint = 0;
        }
        else if (cmd->GetType() == DebuggerCmdType::SetRegister)
        {
            auto setRegister = (DebuggerSetRegisterCmd*)cmd.get();
            
            auto& regDef = g_RegisterMap[setRegister->GetRegisterId()];
            m_Registers->Set(regDef.m_PlayerId, regDef.m_Index, setRegister->GetValue());
        }
        else if (cmd->GetType() == DebuggerCmdType::SetBreakpoint)
        {
            auto setBreakpoint = (DebuggerSetBreakpointCmd*)cmd.get();
            auto& breakpoint = m_Breakpoints[setBreakpoint->GetId()];
            breakpoint.m_Enabled = true;
            breakpoint.m_Address = setBreakpoint->GetAddress();
        }
        else if (cmd->GetType() == DebuggerCmdType::RemoveBreakpoint)
        {
            auto removeBreakpoint = (DebuggerRemoveBreakpointCmd*)cmd.get();
            m_Breakpoints.erase(removeBreakpoint->GetId());
        }
        else if (cmd->GetType() == DebuggerCmdType::ContinueToNext)
        {
            Process::Resume(m_Process);
            m_SuspendOnAddress = m_InstructionCounter;
        }
    }

    IIRInstruction* DebuggerThread::AddressToInstruction(unsigned int address) const
    {
        if (g_AddressToInstructionMap.find(address) == g_AddressToInstructionMap.end())
        {
            return nullptr;
        }

        return *g_AddressToInstructionMap[address].begin();
    }

}
