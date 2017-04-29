#ifndef __LANGUMS_WAVINFO_H
#define __LANGUMS_WAVINFO_H

#include <string>

namespace Langums
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

    class WAVInfo
    {
        public:
        bool ReadInfo(const std::string& path);

        unsigned int GetLengthMilliseconds() const
        {
            return m_LengthMilliseconds;
        }

        const std::vector<char>& GetData() const
        {
            return m_Data;
        }

        private:
        unsigned int m_LengthMilliseconds;
        std::vector<char> m_Data;
    };

}

#endif
