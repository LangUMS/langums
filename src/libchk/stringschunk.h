#ifndef __LIBCHK_STRINGSCHUNK_H
#define __LIBCHK_STRINGSCHUNK_H

#include <unordered_map>
#include <string>

#include "ichunk.h"
#include "../stringhash.h"
#include "../serialization.h"

namespace CHK
{

	class CHKStringsChunk : public IChunk
	{
		public:
		CHKStringsChunk(const std::vector<char>& data, const std::string& type) : IChunk(type)
		{
			SetBytes(data);
		}

		size_t GetStringCount() const
		{
			return m_Offsets.size();
		}

		int FindString(const std::string& s);

		const char* GetString(size_t index) const;

		size_t InsertString(const std::string& s);

		void SetBytes(const std::vector<char>& data);

		private:
		std::vector<uint16_t> m_Offsets;
		std::vector<std::string> m_Strings;
		std::unordered_map<size_t, size_t> m_Hashes;
	};

}

#endif
