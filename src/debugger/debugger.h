#ifndef __LANGUMS_DEBUGGER_H
#define __LANGUMS_DEBUGGER_H

#include <string>
#include <memory>

#include "process_util.h"
#include "reg_table.h"

namespace Langums
{

    class Debugger
    {
        public:
        bool Attach(const std::string& processName);

        bool PauseGame(bool state);

        private:
        std::unique_ptr<RegTable> m_Registers;
        Process::ProcessHandle m_Process;
        void* m_BaseAddress = nullptr;
    };

}

#endif
