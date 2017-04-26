#ifndef __LANGUMS_PREPROCESSOR_H
#define __LANGUMS_PREPROCESSOR_H

#include <string>
#include <unordered_map>

namespace Langums
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

		std::string Process(const std::string& input);

		private:
		std::string ProcessLine(const std::string& line);
		std::string ReadIncludeFile(const std::string& path);

		std::unordered_map<std::string, std::string> m_Defines;
		std::string m_RootFolder;
	};

}

#endif
