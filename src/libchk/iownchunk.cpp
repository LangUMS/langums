#include "enums.h"
#include "iownchunk.h"

namespace CHK
{

    void CHKIOwnChunk::SetBytes(const std::vector<char>& data)
    {
        IChunk::SetBytes(data);
        memcpy(m_PlayerTypes, data.data(), 12);
    }

    void CHKIOwnChunk::SetPlayerType(int playerId, PlayerType type)
    {
        m_PlayerTypes[playerId] = (uint8_t)type;

        std::vector<char> bytes;
        bytes.resize(12);
        memcpy(bytes.data(), m_PlayerTypes, 12);
        IChunk::SetBytes(bytes);
    }

}
