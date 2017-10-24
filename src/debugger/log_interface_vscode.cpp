#include <iostream>

#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "debugger_protocol.h"

#include "log_interface_vscode.h"

namespace LangUMS
{

    using namespace rapidjson;

    LogInterfaceVSCode::LogInterfaceVSCode()
    {
        m_ShutdownThread = 0;
        m_LogThread = std::make_unique<std::thread>(std::bind(&LogInterfaceVSCode::LogThread, this));
    }

    LogInterfaceVSCode::~LogInterfaceVSCode()
    {
        m_ShutdownThread = 1;
        m_MessageInserted.notify_one();
        while (m_ShutdownThread)
        {
        }

        m_LogThread->join();
    }

    void LogInterfaceVSCode::LogMessage(const std::string& message)
    {
        if (message.length() == 0 || (message.length() == 1 && message[0] == '\n'))
        {
            return;
        }

        StringBuffer buf;
        Writer<StringBuffer> writer(buf);

        auto logMsg = std::make_unique<DebuggerProtocol::LogMessage>(message);
        logMsg->Serialize(writer);

        std::unique_lock<std::mutex> _(m_Mutex);
        m_Messages.push_back(buf.GetString());
        m_MessageInserted.notify_one();
    }

    void LogInterfaceVSCode::SendResponse(const std::string& json)
    {
        std::unique_lock<std::mutex> _(m_Mutex);
        m_Messages.push_back(json);
        m_MessageInserted.notify_one();
    }

    void LogInterfaceVSCode::LogThread()
    {
        while (m_ShutdownThread == 0)
        {
            std::unique_lock<std::mutex> _(m_Mutex);

            m_MessageInserted.wait(_, [&]()
            {
                return !m_Messages.empty() || m_ShutdownThread;
            });

            if (m_ShutdownThread == 1)
            {
                break;
            }

            for (auto& message : m_Messages)
            {
                std::cout << message << std::endl;
            }

            m_Messages.clear();
        }

        m_ShutdownThread = 0;
    }

}
