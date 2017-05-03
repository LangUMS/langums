#ifndef __LIBCHK_LANGUMSCHUNK_H
#define __LIBCHK_LANGUMSCHUNK_H

#include <unordered_map>
#include <string>

#include "ichunk.h"
#include "../serialization.h"

#define CHK_LANGUMS_CHUNK_MAGIC_HEADER 0xF3F7AE00
#define CHK_LANGUMS_CHUNK_MAX_REGISTERS 1024
#define CHK_LANGUMS_CHUNK_MAX_SOURCE_LEN 65535

namespace CHK
{

    struct LangChunkData
    {
        unsigned int m_MagicHeader = CHK_LANGUMS_CHUNK_MAGIC_HEADER;

        unsigned int m_UsedRegisters[CHK_LANGUMS_CHUNK_MAX_REGISTERS]; // used registers by the compiler - indexId = (unitId + playerId * 228)
        unsigned int m_UsedRegisterCount = 0;

        char m_Source[CHK_LANGUMS_CHUNK_MAX_SOURCE_LEN]; // langums source
        unsigned int m_SourceLen = 0;
    };

    class CHKLangChunk : public IChunk
    {
        public:
        CHKLangChunk(const std::vector<char>& data, const std::string& type) : IChunk(type)
        {
            SetBytes(data);
        }

        CHKLangChunk(const LangChunkData& data, const std::string& type) : IChunk(type)
        {
            std::vector<char> bytes;
            bytes.resize(sizeof(LangChunkData));
            memcpy(bytes.data(), &data, sizeof(LangChunkData));

            SetBytes(bytes);
        }

        LangChunkData* GetData() const
        {
            auto& bytes = GetBytes();
            return (LangChunkData*)bytes.data();
        }
    };

}

#endif
