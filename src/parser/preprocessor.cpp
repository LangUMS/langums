#include <sstream>

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
			if (line.length() > 0)
			{
				output.append(line);
				output.append("\n");
			}
		}

		return output;
	}

	std::string Preprocessor::ProcessLine(const std::string& line)
	{
		auto commentStart = line.find_first_of("//");
		if (commentStart == std::string::npos)
		{
			return line;
		}

		return line.substr(0, commentStart);
	}

}
