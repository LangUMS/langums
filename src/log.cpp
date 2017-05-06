#include "log.h"

namespace Langums
{
    
    std::unique_ptr<Log> Log::m_Instance = nullptr;

    Log::Log()
    {
        m_ShutdownThread = 0;
        m_LogThread = std::make_unique<std::thread>(std::bind(&Log::LogThread, this));
    }

    void Log::Destroy()
    {
        if (m_LogThread == nullptr)
        {
            return;
        }

        m_ShutdownThread = 1;
        m_MessageInserted.notify_one();
        while (m_ShutdownThread)
        {
        }

        m_LogThread->join();
        m_LogThread = nullptr;
    }

    void Log::LogMessage(const std::string& message)
    {
        std::unique_lock<std::mutex> _(m_Mutex);
        m_Messages.push_back(message);
        m_MessageInserted.notify_one();
    }

    void Log::LogThread()
    {
        while (m_ShutdownThread == 0)
        {
            std::unique_lock<std::mutex> _(m_Mutex);

            m_MessageInserted.wait(_, [&]()
            {
                return !m_Messages.empty() || m_ShutdownThread;
            });

            for (auto& message : m_Messages)
            {
                for (auto& interface : m_Interfaces)
                {
                    interface->LogMessage(message);
                }
            }

            m_Messages.clear();

            if (m_ShutdownThread == 1)
            {
                break;
            }
        }

        m_ShutdownThread = 0;
    }

}
