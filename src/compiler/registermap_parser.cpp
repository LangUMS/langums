#include <sstream>
#include <fstream>
#include <streambuf>

#include "../libchk/src/enums.h"
#include "registermap_parser.h"

namespace LangUMS
{

    std::vector<RegisterDef> RegistermapParser::Parse(const std::string& input)
    {
        std::istringstream iss(input);

        std::vector<RegisterDef> defs;

        auto lineNumber = 1;
        for (std::string line; std::getline(iss, line); )
        {
            ProcessLine(line, defs, lineNumber);
            lineNumber++;
        }

        return defs;
    }

    void RegistermapParser::ProcessLine(const std::string& line, std::vector<RegisterDef>& output, int lineNumber)
    {
        auto trimmed = trim(line);
        if (trimmed.length() == 0)
        {
            return;
        }

        auto comma = trimmed.find_first_of(',');
        if (comma == std::string::npos)
        {
            throw new RegistermapParserException(SafePrintf("Malformed line % in register map - \"%\"", lineNumber, line));
        }

        auto playerName = trim(trimmed.substr(0, comma));
        auto unitName = trim(trimmed.substr(comma + 1));

        auto playerId = -1;
        for (auto i = 0; i < sizeof(CHK::PlayersByName); i++)
        {
            if (CHK::PlayersByName[i] == playerName)
            {
                playerId = i;
                break;
            }
        }

        if (playerId == -1)
        {
            throw new RegistermapParserException(SafePrintf("Invalid player name \"%\" on line % in register map - \"%\"", playerName, lineNumber, line));
        }

        auto unitId = -1;
        for (auto i = 0; i < sizeof(CHK::UnitsByName); i++)
        {
            if (CHK::UnitsByName[i] == unitName)
            {
                unitId = i;
                break;
            }
        }

        if (unitId == -1)
        {
            throw new RegistermapParserException(SafePrintf("Invalid unit name \"%\" on line % in register map - \"%\"", unitName, lineNumber, line));
        }

        RegisterDef def;
        def.m_PlayerId = playerId;
        def.m_Index = unitId;

        output.push_back(def);
    }

}
