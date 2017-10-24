#ifndef __LANGUMS_PREPROCESSOR_H
#define __LANGUMS_PREPROCESSOR_H

#include <string>
#include <unordered_map>

namespace LangUMS
{

    class PreprocessorException : public std::exception
    {
        public:
        PreprocessorException(const char* error) : std::exception(error) {}
        PreprocessorException(const std::string& error) : std::exception(error.c_str()) {}
    };

    class Preprocessor
    {
        public:
        Preprocessor(const std::string& rootFolder) : m_RootFolder(rootFolder) {}

        const std::string& GetMapName() const
        {
            return m_MapName;
        }

        bool HasMapName() const
        {
            return m_HasMapName;
        }

        const std::string& GetOutMapName() const
        {
            return m_OutMapName;
        }

        bool HasOutMapName() const
        {
            return m_HasOutMapName;
        }

        std::string Process(const std::string& input);

        private:
        std::string ProcessLine(const std::string& line);
        std::string ReadIncludeFile(const std::string& path);

        std::unordered_map<std::string, std::string> m_Defines;
        std::string m_RootFolder;

        std::string m_MapName;
        bool m_HasMapName = false;

        std::string m_OutMapName;
        bool m_HasOutMapName = false;
    };

}

#endif
