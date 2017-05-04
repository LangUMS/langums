#ifndef __LANGUMS_LOG_INTERFACE_STDOUT_H
#define __LANGUMS_LOG_INTERFACE_STDOUT_H

#include <iostream>

#include "log.h"

namespace Langums
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
