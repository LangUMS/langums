#ifndef __CHK_H
#define __CHK_H

#include <unordered_map>
#include <memory>
#include <string>
#include <set>

#include "constants.h"
#include "ichunk.h"
#include "verchunk.h"
#include "stringschunk.h"
#include "triggerschunk.h"
#include "locationschunk.h"
#include "ownrchunk.h"
#include "iownchunk.h"
#include "dimchunk.h"
#include "cuwpchunk.h"
#include "cuwpusedchunk.h"
#include "wavchunk.h"
#include "tilesetchunk.h"
#include "langumschunk.h"

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

        void AddChunk(std::unique_ptr<IChunk> chunk)
        {
            m_ChunkTypes.insert(chunk->GetType());
            m_Chunks[chunk->GetType()].push_back(std::move(chunk));
        }

        bool HasChunk(ChunkType type) const
        {
            switch (type)
            {
            case ChunkType::DimChunk:
                return HasChunk("DIM ");
            case ChunkType::IOwnChunk:
                return HasChunk("IOWN");
            case ChunkType::OwnrChunk:
                return HasChunk("OWNR");
            case ChunkType::StringsChunk:
                return HasChunk("STR ");
            case ChunkType::WavChunk:
                return HasChunk("WAV ");
            case ChunkType::VerChunk:
                return HasChunk("VER ");
            case ChunkType::TriggersChunk:
                return HasChunk("TRIG");
            case ChunkType::TilesetsChunk:
                return HasChunk("ERA ");
            case ChunkType::LocationsChunk:
                return HasChunk("MRGN");
            case ChunkType::CuwpChunk:
                return HasChunk("UPRP");
            case ChunkType::CuwpUsedChunk:
                return HasChunk("UPUS");
            case ChunkType::LangumsChunk:
                return HasChunk("LANG");
            }

            return false;
        }

        template <typename T>
        T* GetFirstChunk(ChunkType type) const
        {
            switch (type)
            {
                case ChunkType::DimChunk:
                    return GetFirstChunk<T>("DIM ");
                case ChunkType::IOwnChunk:
                    return GetFirstChunk<T>("IOWN");
                case ChunkType::OwnrChunk:
                    return GetFirstChunk<T>("OWNR");
                case ChunkType::StringsChunk:
                    return GetFirstChunk<T>("STR ");
                case ChunkType::WavChunk:
                    return GetFirstChunk<T>("WAV ");
                case ChunkType::VerChunk:
                    return GetFirstChunk<T>("VER ");
                case ChunkType::TriggersChunk:
                    return GetFirstChunk<T>("TRIG");
                case ChunkType::TilesetsChunk:
                    return GetFirstChunk<T>("ERA ");
                case ChunkType::LocationsChunk:
                    return GetFirstChunk<T>("MRGN");
                case ChunkType::CuwpChunk:
                    return GetFirstChunk<T>("UPRP");
                case ChunkType::CuwpUsedChunk:
                    return GetFirstChunk<T>("UPUS");
                case ChunkType::LangumsChunk:
                    return GetFirstChunk<T>("LANG");
            }

            return nullptr;
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

        const std::set<std::string>& GetChunkTypes() const
        {
            return m_ChunkTypes;
        }

        private:
        bool HasChunk(const std::string& type) const
        {
            return m_Chunks.find(type) != m_Chunks.end();
        }


        template <typename T>
        T* GetFirstChunk(const std::string& type) const
        {
            auto it = m_Chunks.find(type);
            if (it == m_Chunks.end())
            {
                return nullptr;
            }

            return (T*)(*it).second.front().get();
        }

        std::unordered_map<std::string, std::vector<std::unique_ptr<IChunk>>> m_Chunks;
        std::set<std::string> m_ChunkTypes;
    };

}

#endif
