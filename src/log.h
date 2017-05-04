#ifndef __LOG_H
#define __LOG_H

#define LOG(s) ::Langums::Log::Instance()->LogMessage(s)
#define LOG_F(s, ...) ::Langums::Log::Instance()->LogMessage(SafePrintf(s, __VA_ARGS__))
#define LOG_DEINIT() ::Langums::Log::Instance()->Destroy()
#define LOG_EXITERR(s, ...) LOG_F(s, __VA_ARGS__); ::Langums::Log::Instance()->Destroy()

#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <string>
#include <vector>
#include <memory>

#include "stringutil.h"

namespace Langums
{

    class ILogInterface
    {
        public:
        virtual ~ILogInterface()
        {}

        virtual void LogMessage(const std::string& message) = 0;
    };

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

        void AddInterface(std::unique_ptr<ILogInterface> interface)
        {
            m_Interfaces.push_back(std::move(interface));
        }

        void LogMessage(const std::string& message);

        private:
        void LogThread();
        std::atomic_char m_ShutdownThread;
        std::condition_variable m_MessageInserted;
        std::mutex m_Mutex;
        std::unique_ptr<std::thread> m_LogThread;

        static std::unique_ptr<Log> m_Instance;
        std::vector<std::string> m_Messages;

        std::vector<std::unique_ptr<ILogInterface>> m_Interfaces;
    };

}

#endif
