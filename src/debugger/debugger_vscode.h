#ifndef __LANGUMS_DEBUGGER_VSCODE_H
#define __LANGUMS_DEBUGGER_VSCODE_H

#include <memory>
#include <thread>
#include <atomic>

#include "debugger.h"
#include "log_interface_vscode.h"
#include "debugger_protocol.h"

namespace Langums
{

    class DebuggerVSCode
    {
        public:
        DebuggerVSCode()
        {}

        ~DebuggerVSCode();

        void SetDebugger(Debugger* debugger)
        {
            m_Debugger = debugger;
        }

        void SetLogInterface(LogInterfaceVSCode* logInterface)
        {
            m_LogInterface = logInterface;
        }

        bool Run();

        private:
        void ProcessCommand(const std::string& json);

        void SendResponse(const std::string& json)
        {
            m_LogInterface->SendResponse(json);
        }

        void SendEvent(DebuggerProtocol::IDebuggerEvent* event);

        void RunEventsThread();

        std::vector<std::string> m_SourceLines;
        
        Debugger* m_Debugger = nullptr;
        LogInterfaceVSCode* m_LogInterface = nullptr;

        std::unique_ptr<std::thread> m_EventsThread;
        std::atomic<bool> m_ShutdownEventsThread;
    };

}

#endif
