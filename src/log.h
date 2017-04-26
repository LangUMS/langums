#ifndef __LOG_H
#define __LOG_H

#define LOG(s) ::Log::Log::Instance()->LogMessage(s)
#define LOG_F(s, ...) ::Log::Log::Instance()->LogMessage(::SafePrintf(s, __VA_ARGS__))
#define LOG_DEINIT() ::Log::Log::Instance()->Destroy()
#define LOG_EXITERR(s, ...) LOG_F(s, __VA_ARGS__); ::Log::Log::Instance()->Destroy()

#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>

#include "stringutil.h"

namespace Log
{

    class Log
    {

        public:
        static const std::unique_ptr<Log>& Instance()
        {
            if (m_Instance == nullptr)
            {
                m_Instance = std::make_unique<Log>();
            }

            return m_Instance;
        }

        Log();

        ~Log()
        {
            Destroy();
        }

        void Destroy();

        void LogMessage(const std::string& message);

        private:
        void LogThread();
        std::atomic_char m_ShutdownThread;
        std::condition_variable m_MessageInserted;
        std::mutex m_Mutex;
        std::unique_ptr<std::thread> m_LogThread;

        static std::unique_ptr<Log> m_Instance;
        std::vector<std::string> m_Messages;

        std::unique_ptr<std::ofstream> m_LogFile;

    };

}

#endif
