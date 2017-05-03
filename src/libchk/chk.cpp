#include <vector>

#include "chk.h"

namespace CHK
{

    File::File(const std::vector<char>& data, bool stripExtraData)
    {
        auto i = 0u;
        while (i < data.size())
        {
            std::string type(data.data() + i, 4);
            i += 4;
            auto size = *((uint32_t*)(data.data() + i));
            i += 4;

            std::vector<char> bytes(data.data() + i, data.data() + i + size);
            i += size;

            std::unique_ptr<IChunk> chunk = nullptr;
            
            m_ChunkTypes.insert(type);

            if (type == "LANG")
            {
                chunk = std::make_unique<CHKLangChunk>(bytes, type);
            }
            else if (type == "VER ")
            {
                chunk = std::make_unique<CHKVerChunk>(bytes, type);
            }
            else if (type == "STR ")
            {
                chunk = std::make_unique<CHKStringsChunk>(bytes, type);
            }
            else if (type == "TRIG")
            {
                chunk = std::make_unique<CHKTriggersChunk>(bytes, type);
            }
            else if (type == "MRGN")
            {
                chunk = std::make_unique<CHKLocationsChunk>(bytes, type);
            }
            else if (type == "OWNR")
            {
                chunk = std::make_unique<CHKOwnrChunk>(bytes, type);
            }
            else if (type == "IOWN")
            {
                chunk = std::make_unique<CHKIOwnChunk>(bytes, type);
            }
            else if (type == "DIM ")
            {
                chunk = std::make_unique<CHKDimChunk>(bytes, type);
            }
            else if (type == "UPRP")
            {
                chunk = std::make_unique<CHKCuwpChunk>(bytes, type);
            }
            else if (type == "UPUS")
            {
                chunk = std::make_unique<CHKCuwpUsedChunk>(bytes, type);
            }
            else if (type == "WAV ")
            {
                chunk = std::make_unique<CHKWavChunk>(bytes, type);
            }
            else if (type == "ERA ")
            {
                chunk = std::make_unique<CHKTilesetChunk>(bytes, type);
            }
            else if
            ( // skip all non-required chunks to lower file-size
                type == "TYPE" ||
                type == "IVER" ||
                type == "IVE2" ||
                type == "IOWN" ||
                type == "UPGR" ||
                type == "PTEC" ||
                type == "ISOM" ||
                type == "TILE" ||
                type == "DD2 " ||
                type == "UPUS" ||
                type == "UNIS" ||
                type == "UPGS" ||
                type == "TECS"
            )
            {
                if (stripExtraData)
                {
                    continue;
                }
                else
                {
                    chunk = std::make_unique<IChunk>(bytes, type);
                }
            }
            else
            {
                chunk = std::make_unique<IChunk>(bytes, type);
            }

            if (chunk != nullptr)
            {
                m_Chunks[type].push_back(std::move(chunk));
            }
        }
    }

}
