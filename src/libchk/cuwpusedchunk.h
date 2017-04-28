#ifndef __LIBCHK_CUWPUSEDCHUNK_H
#define __LIBCHK_CUWPUSEDCHUNK_H

#include <unordered_map>
#include <string>

#include "ichunk.h"
#include "../serialization.h"

namespace CHK
{

    class CHKCuwpUsedChunk : public IChunk
    {
        public:
        CHKCuwpUsedChunk(const std::vector<char>& data, const std::string& type) : IChunk(type)
        {
            SetBytes(data);
            memcpy(m_UsedSlots, data.data(), 64);
        }

        unsigned int GetCount() const
        {
            return 64;
        }

        int FindFreeSlot() const
        {
            for (auto i = 0u; i < 64u; i++)
            {
                if (!IsUsed(i))
                {
                    return i;
                }
            }

            return -1;
        }

        bool IsUsed(unsigned int index) const
        {
            if (index >= 64)
            {
                return true;
            }

            auto& bytes = GetBytes();
            return bytes[index] != 0;
        }

        void SetUsed(unsigned int index, bool used)
        {
            if (index >= 64)
            {
                return;
            }

            auto& bytes = GetBytes();
            *(char*)(bytes.data() + index) = used ? 1 : 0;
            m_UsedSlots[index] = used ? 1 : 0;
        }

        uint8_t m_UsedSlots[64];
    };

}

#endif
