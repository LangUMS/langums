#ifndef __LANGUMS_PREPROCESSOR_H
#define __LANGUMS_PREPROCESSOR_H

#include <string>

namespace Langums
{

	class Preprocessor
	{
		public:
		std::string Process(const std::string& input);

		private:
		std::string ProcessLine(const std::string& line);
	};

}

#endif
