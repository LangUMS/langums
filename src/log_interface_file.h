#ifndef __LANGUMS_LOG_INTERFACE_FILE_H
#define __LANGUMS_LOG_INTERFACE_FILE_H

#include <iostream>
#include <fstream>

#include "log.h"

namespace Langums
{

    class LogInterfaceFile : public ILogInterface
    {
        public:
        LogInterfaceFile(const std::string& path) : m_File(path) {}

        void LogMessage(const std::string& message)
        {
            m_File << message << std::endl;
            m_File.flush();
        }

        private:
        std::ofstream m_File;
    };

}

#endif
