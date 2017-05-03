#ifndef __LANGUMS_DEBUGGER_H
#define __LANGUMS_DEBUGGER_H

#include <string>
#include <memory>

#include "reg_table.h"

namespace Langums
{

    class Debugger
    {
        public:
        bool Attach(const std::string& processName);

        private:
        std::unique_ptr<RegTable> m_Registers;
    };

}

#endif
