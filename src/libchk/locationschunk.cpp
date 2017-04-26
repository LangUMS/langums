#include "locationschunk.h"

namespace CHK
{

    void CHKLocationsChunk::SetBytes(const std::vector<char>& data)
    {
        IChunk::SetBytes(data);
    }

    int CHKLocationsChunk::FindLocation(unsigned int stringIndex) const
    {
        auto count = GetLocationsCount();
        for (auto i = 0u; i < count; i++)
        {
            auto loc = GetLocation(i);
            if (loc && loc->m_StringIndex == stringIndex)
            {
                return i;
            }
        }

        return -1;
    }

}
