#include "dimchunk.h"

namespace CHK
{

    void CHKDimChunk::SetBytes(const std::vector<char>& data)
    {
        m_Width = *((uint16_t*)data.data());
        m_Height = *((uint16_t*)(data.data() + 2));
        IChunk::SetBytes(data);
    }

}