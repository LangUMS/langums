#include <fstream>
#include <vector>

#include "wavinfo.h"

namespace LangUMS
{

    struct WAVHeader
    {
        uint32_t m_ChunkId;
        uint32_t m_ChunkDataSize;
        uint32_t m_RiffType;
        uint32_t m_Fmt;
        uint32_t m_FmtChunkSize;
        uint16_t m_FormatTag;
        uint16_t m_Channels;
        uint32_t m_SamplesPerSecond;
        uint32_t m_BytesPerSecond;
        uint16_t m_BlockAlign;         /* 1 => 8-bit mono, 2 => 8-bit stereo or 16-bit mono, 4 => 16-bit stereo */
        uint16_t m_BitsPerSample;
    };

    struct FactHeader
    {
        uint32_t m_ChunkId;
        uint32_t m_ChunkDataSize;
        uint32_t m_SampleLength;
    };

    struct DataHeader
    {
        uint32_t m_Id;
        uint32_t m_Size;
    };

    bool WAVInfo::ReadInfo(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        m_Data.resize((unsigned int)size);
        if (!file.read(m_Data.data(), size))
        {
            return false;
        }

        auto start = m_Data.data();

        auto wavHeader = (WAVHeader*)start;
        auto dataSize = *((uint32_t*)(start + sizeof(WAVHeader) + 4));

        auto bytesPerSample = wavHeader->m_BitsPerSample / 8;
        auto bytesPerSecond = wavHeader->m_SamplesPerSecond * bytesPerSample * wavHeader->m_Channels;
        
        m_LengthMilliseconds = (dataSize * 1000) / bytesPerSecond;
        
        file.close();
        return true;
    }

}