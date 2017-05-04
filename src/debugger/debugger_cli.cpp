#include <iostream>
#include <string>

#include "debugger_cli.h"
#include "../pretty_errors.h"

namespace Langums
{

    bool DebuggerCli::Repl()
    {
        using namespace std::chrono;

        auto before = high_resolution_clock::now();
        while (m_Debugger->GetState() != DebuggerThreadState::Attached)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            auto now = high_resolution_clock::now();
            if ((now - before) > seconds(90))
            {
                return false;
            }
        }

        auto sourceLines = Split(m_Debugger->GetSource());
        std::cout << std::endl;

        while (true)
        {
            if (m_Debugger->GetState() == DebuggerThreadState::NotRunning)
            {
                std::cout << "Shutdown." << std::endl;
                return true;
            }

            std::cout << "(ldb) ";

            std::string line;
            
            if (!std::getline(std::cin, line))
            {
                return true;
            }

            if (line == "brk")
            {
                Brk();
            }
            else if (line == "res")
            {
                Res();
            }
            else if (line == "ic")
            {
                IC();
            }
            else if (line == "ir")
            {
                IR();
            }
            else if (line == "where")
            {
                Where();
            }
            else if (line == "reg")
            {
                Reg();
            }
            else if (line == "next")
            {
                Next();
            }
            else
            {
                std::cout << "Invalid command.";
            }

            std::cout << std::endl;
        }

        return true;
    }

    void DebuggerCli::Brk()
    {
        m_Debugger->SuspendExecution();
    }

    void DebuggerCli::Res()
    {
        m_Debugger->ResumeExecution();
    }

    void DebuggerCli::IC()
    {
        std::cout << m_Debugger->GetCurrentAddress();
    }

    void DebuggerCli::IR()
    {
        auto instruction = m_Debugger->GetCurrentInstruction();
        if (instruction == nullptr)
        {
            std::cout << "Unknown instruction.";
        }
        else
        {
            std::cout << instruction->DebugDump();
        }
    }

    void DebuggerCli::Where()
    {
        auto lines = Split(m_Debugger->GetSource());

        auto charIndex = m_Debugger->GetCurrentSourceCharIndex();
        if (charIndex == 0)
        {
            std::cout << "Unknown.";
        }
        else
        {
            auto lineNumber = GetLineNumber(m_Debugger->GetSource(), charIndex);

            for (auto i = 0u; i < lineNumber; i++)
            {
                charIndex -= lines[i].length();
            }

            std::cout << "Line: " << lineNumber - 1 << ", Char: " << (int)charIndex;

            std::cout << ">>> " << trim(lines[lineNumber - 1]);
        }
    }

    void DebuggerCli::Reg()
    {
        auto instruction = m_Debugger->GetCurrentInstruction();
        
    }

    void DebuggerCli::Next()
    {
        m_Debugger->ContinueToNextAddress();
        Where();
    }

}
