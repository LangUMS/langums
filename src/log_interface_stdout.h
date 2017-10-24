#ifndef __LANGUMS_LOG_INTERFACE_STDOUT_H
#define __LANGUMS_LOG_INTERFACE_STDOUT_H

#include <iostream>

#include "log.h"

#define LOG_ADD_STDOUT_IFACE() Log::Instance()->AddInterface(std::unique_ptr<ILogInterface>(new LogInterfaceStdout()))

namespace LangUMS
{

    class LogInterfaceStdout : public ILogInterface
    {
        public:
        void LogMessage(const std::string& message)
        {
            std::cout << message << std::endl;
        }
    };

}

#endif
