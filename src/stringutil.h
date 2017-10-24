#ifndef __LANGUMS_STRINGUTIL_H
#define __LANGUMS_STRINGUTIL_H

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#undef min
#undef max

namespace LangUMS
{

    inline std::vector<std::string> Split(const std::string& src)
    {
        std::stringstream ss(src);
        std::string line;

        std::vector<std::string> lines;

        while (std::getline(ss, line, '\n'))
        {
            lines.push_back(line);
        }

        return lines;
    }

    inline unsigned int GetLineNumber(const std::string& src, int charIndex)
    {
        auto lineNumber = 1;
        for (auto i = 0; i < std::min((int)src.length(), charIndex); i++)
        {
            if (src[i] == '\n')
            {
                lineNumber++;
            }
        }

        return lineNumber;
    }

    static std::string SafePrintf(const char *s)
    {
        std::stringstream ss;

        while (*s) {
            if (*s == '%') {
                if (*(s + 1) == '%') {
                    ++s;
                }
                else {
                    throw "invalid format string: missing arguments";
                }
            }
            ss << *s++;
        }

        return ss.str();
    }

    template<typename T, typename... Args>
    static std::string SafePrintf(const char* s, const T& value, Args... args)
    {
        std::stringstream ss;

        while (*s)
        {
            if (*s == '%')
            {
                if (*(s + 1) == '%')
                {
                    ++s;
                }
                else
                {
                    ss << value;
                    ss << SafePrintf(s + 1, args...);
                    return ss.str();
                }
            }

            ss << *s++;
        }

        throw "Too many arguments provided to SafePrintf";
    }

    inline std::string trim(const std::string& s)
    {
        auto line = s;
        auto end = line.find_last_not_of(" \t");
        if (std::string::npos != end)
        {
            line = line.substr(0, end + 1);
        }

        size_t start = line.find_first_not_of(" \t");
        if (std::string::npos != start)
        {
            line = line.substr(start);
        }

        return line;
    }

}

#endif
