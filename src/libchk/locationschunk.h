#ifndef __LIBCHK_LOCATIONSCHUNK_H
#define __LIBCHK_LOCATIONSCHUNK_H

#include <unordered_map>
#include <string>

#include "ichunk.h"
#include "../serialization.h"

#define ANYWHERE_LOCATION 64

namespace CHK
{

	enum class LocationElevation
	{
		Low = (1 << 0),
		Medium = (1 << 1),
		High = (1 << 2),
		LowAir = (1 << 3),
		MediumAir = (1 << 4),
		HighAir = (1 << 5)
	};

	struct Location
	{
		uint32_t m_Left;
		uint32_t m_Top;
		uint32_t m_Right;
		uint32_t m_Bottom;
		uint16_t m_StringIndex;
		uint16_t m_Elevation;
	};

	class CHKLocationsChunk : public IChunk
	{
		public:
		CHKLocationsChunk(const std::vector<char>& data, const std::string& type) : IChunk(type)
		{
			SetBytes(data);
		}

		int FindLocation(unsigned int stringIndex) const;

		size_t GetLocationsCount() const
		{
			return 255;
		}

		Location* GetLocation(size_t index) const
		{
			if (index >= 255 || index == ANYWHERE_LOCATION)
			{
				return nullptr;
			}

			auto& bytes = GetBytes();
			return (Location*)(bytes.data() + sizeof(Location) * index);
		}
		
		void SetLocation(size_t index, const Location& location)
		{
			auto& bytes = GetBytes();
			auto loc = (Location*)(bytes.data() + sizeof(Location) * index);
			*loc = location;
		}

		void SetBytes(const std::vector<char>& data);
	};

}

#endif
