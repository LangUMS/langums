#include <iostream>
#include <chrono>

#include "../log.h"
#include "../compiler/compiler.h"

#include "process_util.h"
#include "memsearch.h"
#include "reg_table.h"
#include "debugger.h"

#undef min
#undef max

#define VERSION "v0.1.0"

#define BASE_OFFSET 0x00e50000

#define SCENARIO_FILENAME "staredit\\scenario.chk"

namespace Langums
{

    bool Debugger::Attach(const std::string& processName)
    {
        LOG_F("Debugger: Looking for \"%\"", processName);

        void* baseAddress;

        auto before = std::chrono::high_resolution_clock::now();

        Process::ProcessHandle handle = nullptr;
        while (handle == nullptr)
        {
            handle = Process::OpenByName(processName, baseAddress);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            auto now = std::chrono::high_resolution_clock::now();

            if ((now - before) > std::chrono::seconds(90))
            {
                LOG_EXITERR("\n(!) Debugger: Failed to find \"%\" for 90 seconds, bailing out...", processName);
                return 1;
            }
        }

        LOG_F("Debugger: Attached to process.");

        m_Registers = std::make_unique<RegTable>(handle, baseAddress);

        auto& icDef = g_RegisterMap[Reg_InstructionCounter];

        std::chrono::high_resolution_clock timer;
        before = timer.now();

        while (true)
        {
            auto now = timer.now();
            if ((now - before) >= std::chrono::milliseconds(50))
            {
                m_Registers->Update();

                auto icValue = m_Registers->Get(icDef.m_PlayerId, icDef.m_Index);
                LOG_F("%", icValue);
                before = now;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

}
