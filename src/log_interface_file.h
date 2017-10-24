#ifndef __LANGUMS_LOG_INTERFACE_FILE_H
#define __LANGUMS_LOG_INTERFACE_FILE_H

#include <iostream>
#include <fstream>

#include "log.h"

#define LOG_ADD_FILE_IFACE(PATH) Log::Instance()->AddInterface(std::unique_ptr<ILogInterface>(new LogInterfaceFile(PATH)))

namespace LangUMS
{

    class LogInterfaceFile : public ILogInterface
    {
        public:
        LogInterfaceFile(const std::string& path) : m_File(path)
        {
            if (!m_File.is_open())
            {
                throw LogInterfaceException(SafePrintf("Failed to open \"%\" for writing.", path));
            }
        }

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
