#ifndef __LANGUMS_PREPROCESSOR_H
#define __LANGUMS_PREPROCESSOR_H

#include <string>

namespace Langums
{

	class PreprocessorException : public std::exception
	{
		public:
		PreprocessorException(int charPosition, const char* error) : m_CharPosition(charPosition), std::exception(error) {}
		PreprocessorException(int charPosition, const std::string& error) : m_CharPosition(charPosition), std::exception(error.c_str()) {}

		int GetCharPosition() const
		{
			return m_CharPosition;
		}

		private:
		int m_CharPosition = 0;
	};

	class Preprocessor
	{
		public:
		std::string Process(const std::string& input);

		private:
		std::string ProcessLine(const std::string& line);
	};

}

#endif
