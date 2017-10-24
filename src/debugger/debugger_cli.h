#ifndef __DEBUGGER_CLI_H
#define __DEBUGGER_CLI_H

#include "debugger.h"

namespace LangUMS
{

    class DebuggerCli
    {
        public:
        DebuggerCli(Debugger* debugger) : m_Debugger(debugger)
        {}

        bool Repl();

        private:
        void Brk();
        void Res();
        void IC();
        void IR();
        void Where();
        void Reg();
        void Next();

        Debugger* m_Debugger = nullptr;
    };

}

#endif
