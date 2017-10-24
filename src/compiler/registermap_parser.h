#ifndef __LANGUMS_REGISTERMAP_PARSER
#define __LANGUMS_REGISTERMAP_PARSER

#include <vector>
#include "triggerbuilder.h"

namespace LangUMS
{

    class RegistermapParserException : public std::exception
    {
        public:
        RegistermapParserException(const char* error) : std::exception(error) {}
        RegistermapParserException(const std::string& error) : std::exception(error.c_str()) {}
    };

    class RegistermapParser
    {
        public:
        std::vector<RegisterDef> Parse(const std::string& input);

        private:
        void ProcessLine(const std::string& line, std::vector<RegisterDef>& output, int lineNumber);
    };

}

#endif
