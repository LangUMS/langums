#ifndef __LANGUMS_LOG_INTERFACE_VSCODE_H
#define __LANGUMS_LOG_INTERFACE_VSCODE_H

#include "../log.h"

namespace Langums
{

    class LogInterfaceVSCode : public ILogInterface
    {
        public:
        LogInterfaceVSCode();
        ~LogInterfaceVSCode();

        void LogMessage(const std::string& message);
        void SendResponse(const std::string& json);

        private:
        void LogThread();
        std::atomic_char m_ShutdownThread;
        std::condition_variable m_MessageInserted;
        std::mutex m_Mutex;
        std::unique_ptr<std::thread> m_LogThread;
        std::vector<std::string> m_Messages;
    };

}

#endif
