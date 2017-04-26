#ifndef __CHK_H
#define __CHK_H

#include <unordered_map>
#include <memory>
#include <string>

#include "constants.h"
#include "ichunk.h"
#include "verchunk.h"
#include "stringschunk.h"
#include "triggerschunk.h"
#include "locationschunk.h"
#include "ownrchunk.h"

namespace CHK
{

    class File
    {
        public:
        File(const std::vector<char>& data, bool stripExtraData);

        size_t GetSize() const
        {
            size_t size = 0;
            for (auto& pair : m_Chunks)
            {
                for (auto& chunk : pair.second)
                {
                    size += 8; // chunk id + chunk size
                    size += chunk->GetBytes().size();
                }
            }
            
            return size;
        }

        void Serialize(std::vector<char>& outBytes)
        {
            outBytes.resize(GetSize());

            size_t offset = 0u;

            for (auto& pair : m_Chunks)
            {
                for (auto& chunk : pair.second)
                {
                    memcpy(outBytes.data() + offset, chunk->GetType().c_str(), 4);
                    offset += 4;

                    auto& bytes = chunk->GetBytes();
                    auto size = (uint32_t)bytes.size();

                    memcpy(outBytes.data() + offset, &size, 4);
                    offset += 4;

                    memcpy(outBytes.data() + offset, bytes.data(), bytes.size());
                    offset += bytes.size();
                }
            }
        }

        bool HasChunk(const std::string& type) const
        {
            return m_Chunks.find(type) != m_Chunks.end();
        }

        template <typename T>
        T& GetFirstChunk(const std::string& type) const
        {
            auto it = m_Chunks.find(type);
            if (it == m_Chunks.end())
            {
                return *(T*)nullptr;
            }

            return *((T*)((*it).second.front().get()));
        }

        template <typename T>
        std::vector<T*> GetChunks(const std::string& type)
        {
            std::vector<IChunk> chunks;

            auto it = m_Chunks.find(type);
            if (it == m_Chunks.end())
            {
                return chunks;
            }

            for (auto& chunk : (*it).second)
            {
                chunks.push_back((T*)chunk.get());
            }

            return chunks;
        }

        size_t GetChunkCount() const
        {
            return m_Chunks.size();
        }

        private:
        std::unordered_map<std::string, std::vector<std::unique_ptr<IChunk>>> m_Chunks;
    };

}

#endif
