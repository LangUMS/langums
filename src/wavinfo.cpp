#include <fstream>
#include <vector>

#include "wavinfo.h"

namespace Langums
{

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