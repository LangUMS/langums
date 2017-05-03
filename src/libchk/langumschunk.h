#ifndef __LIBCHK_LANGUMSCHUNK_H
#define __LIBCHK_LANGUMSCHUNK_H

#include <unordered_map>
#include <string>

#include "ichunk.h"
#include "../serialization.h"

namespace CHK
{

    struct LangChunkData
    {
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
