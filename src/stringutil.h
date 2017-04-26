#ifndef __LANGUMS_STRINGUTIL_H
#define __LANGUMS_STRINGUTIL_H

#include <string>
#include <sstream>

namespace Langums
{

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
