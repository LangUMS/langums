#include <sstream>
#include <fstream>
#include <streambuf>
#include <experimental/filesystem>

#include "../stringutil.h"

#include "preprocessor.h"

namespace Langums
{

    std::string Preprocessor::Process(const std::string& input)
    {
        std::istringstream iss(input);
        std::string output;

        for (std::string line; std::getline(iss, line); )
        {
            line = ProcessLine(line);
            output.append(line);
            output.append("\n");
        }

        return output;
    }

    std::string Preprocessor::ProcessLine(const std::string& line)
    {
        using namespace std::experimental;

        auto trimmed = trim(line);
        if (trimmed[0] == '#')
        {
            auto space = trimmed.find_first_of(' ');
            auto cmd = trimmed.substr(0, space);
            if (cmd == "#define")
            {
                trimmed = trim(trimmed.substr(space));
                space = trimmed.find_first_of(' ');
                auto key = trimmed.substr(0, space);
                auto value = trim(trimmed.substr(space));

                m_Defines.erase(key);
                m_Defines.insert(std::make_pair(key, value));
                return "";
            }
            else if (cmd == "#undef")
            {
                trimmed = trim(trimmed.substr(space));
                space = trimmed.find_first_of(' ');
                auto key = trimmed.substr(0, space);
                m_Defines.erase(key);
            }
            else if (cmd == "#include")
            {
                trimmed = trim(trimmed.substr(space));
                auto path = filesystem::path(m_RootFolder);
                path.append(trimmed);

                if (!filesystem::is_regular_file(path))
                {
                    throw new PreprocessorException(SafePrintf("Failed to #include from \"%\"", trimmed));
                }

                return ReadIncludeFile(path.generic_u8string());
            }
            else if (cmd == "#src")
            {
                trimmed = trim(trimmed.substr(space));
                auto path = filesystem::path(m_RootFolder);
                path.append(trimmed);
                m_HasMapName = true;
                m_MapName = path.generic_u8string();
                return "";
            }
            else if (cmd == "#dst")
            {
                trimmed = trim(trimmed.substr(space));
                auto path = filesystem::path(m_RootFolder);
                path.append(trimmed);
                m_HasOutMapName = true;
                m_OutMapName = path.generic_u8string();
                return "";
            }
        }

        auto outLine = line;

        for (auto& pair : m_Defines)
        {
            auto& search = pair.first;
            auto& replace = pair.second;

            auto pos = 0u;
            while ((pos = outLine.find(search, pos)) != std::string::npos)
            {
                outLine.replace(pos, search.length(), replace);
                pos += replace.length();
            }
        }

        auto commentStart = outLine.find("//");
        if (commentStart == std::string::npos)
        {
            return outLine;
        }

        return outLine.substr(0, commentStart);
    }

    std::string Preprocessor::ReadIncludeFile(const std::string& path)
    {
        return Process(std::string((std::istreambuf_iterator<char>(std::ifstream(path))), std::istreambuf_iterator<char>()));
    }

}
