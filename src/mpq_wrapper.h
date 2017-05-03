#ifndef __LANGUMS_MPQ_WRAPPER_H
#define __LANGUMS_MPQ_WRAPPER_H

#include <string>

namespace Langums
{

    typedef void* HANDLE;

    class MPQWrapperException : public std::exception
    {
        public:
        MPQWrapperException(const char* error) : std::exception(error)
        {}
        MPQWrapperException(const std::string& error) : std::exception(error.c_str())
        {}
    };

    class MPQWrapper
    {
        public:
        MPQWrapper(const std::string& path, bool readOnly);
        ~MPQWrapper();

        void ReadFile(const std::string& filename, std::vector<char>& retBytes);
        void WriteFile(const std::string& filename, const std::vector<char>& bytes, bool compress);
        void AddWavFile(const std::string& path, const std::string& mpqFilename);

        private:
        std::string m_Path;
        bool m_ReadOnly = false;
        HANDLE m_MPQ;
    };

}

#endif
