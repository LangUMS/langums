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

    Debugger::~Debugger()
    {
        if (m_Thread == nullptr)
        {
            return;
        }

        m_ShutdownThread = true;
        while (m_ShutdownThread)
        {
        }

        m_Thread->join();
    }

    bool Debugger::Attach(const std::string& processName)
    {
        if (m_Thread != nullptr)
        {
            return false;
        }

        m_ProcessName = processName;
        m_Thread = std::make_unique<std::thread>(std::bind(&Debugger::RunThread, this));
        return true;
    }

    void Debugger::Detach()
    {
        if (m_Thread == nullptr)
        {
            return;
        }

        if (!m_ShutdownThread)
        {
            m_ShutdownThread = true;
            while (m_ShutdownThread)
            {
            }
        }

        m_Thread->join();
        m_Thread = nullptr;
    }

    void Debugger::RunThread()
    {
        using namespace std::chrono;

        LOG_F("Debugger: Looking for \"%\"", m_ProcessName);

        auto before = high_resolution_clock::now();
        
        m_State = DebuggerState::WaitingForProcess;

        m_Process = nullptr;
        while (m_Process == nullptr)
        {
            m_Process = Process::OpenByName(m_ProcessName, m_BaseAddress);
            std::this_thread::sleep_for(milliseconds(500));

            auto now = high_resolution_clock::now();

            if ((now - before) > seconds(90))
            {
                LOG_F("\n(!) Debugger: Failed to find \"%\" for 90 seconds, bailing out...", m_ProcessName);
                m_State = DebuggerState::NotRunning;
                m_ShutdownThread = true;
                return;
            }
        }

        m_State = DebuggerState::Attached;

        m_Registers = std::make_unique<RegTable>(m_Process, m_BaseAddress);
        LOG_F("Debugger: Attached to process.");

        auto updateRate = 1.0 / (double)m_PollRate;
            
        before = high_resolution_clock::now();
        auto acc = 0.0;

        while (true)
        {
            auto now = high_resolution_clock::now();
            auto dt = (double)duration_cast<milliseconds>(now - before).count() / 1000.0;
            before = now;

            acc += dt;
            if (acc >= updateRate)
            {
                m_Registers->Update();

                auto& icDef = g_RegisterMap[Reg_InstructionCounter];
                m_InstructionCounter = m_Registers->Get(icDef.m_PlayerId, icDef.m_Index);
                acc -= updateRate;
            }

            if (dt < updateRate)
            {
                std::this_thread::sleep_for(milliseconds((size_t)((updateRate - dt) * 1000.0)));
            }

            if (m_ShutdownThread || !Process::IsRunning(m_Process))
            {
                m_ShutdownThread = false;
                break;
            }
        }
    }

    void Debugger::PauseGame(bool state)
    {
        if (state)
        {
            Process::Suspend(m_Process);
        }
        else
        {
            Process::Resume(m_Process);
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

}
