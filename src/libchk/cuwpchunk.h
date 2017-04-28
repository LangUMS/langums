#ifndef __LIBCHK_CUWPCHUNK_H
#define __LIBCHK_CUWPCHUNK_H

#include <unordered_map>
#include <string>

#include "ichunk.h"
#include "../serialization.h"

namespace CHK
{

    enum class CUWPSpecialPropertiesFlag
    {
        CloakIsValid = (1 << 0),
        BurrowIsValid = (1 << 1),
        InTransitIsValid = (1 << 2),
        HallucinatedIsValid = (1 << 3),
        InvincibleIsValid = (1 << 4)
    };

    enum class CUWPDataElementFlag
    {
        OwnerPlayerIsValid = (1 << 0),
        HitPointsIsValid = (1 << 1),
        ShieldPointsIsValid = (1 << 2),
        EnergyIsValid = (1 << 3),
        ResourceAmountIsValid = (1 << 4),
        HangarCountIsValid = (1 << 5)
    };

    enum class CUWPFlag
    {
        Cloaked = (1 << 0),
        Burrowed = (1 << 1),
        InTransit = (1 << 2),
        Hallucinated = (1 << 3),
        Invincible = (1 << 4)
    };

    struct CUWPSlot
    {
        uint16_t m_ValidSpecialProperties;
        uint16_t m_ValidDataElements;
        uint8_t m_OwnerId;
        uint8_t m_HitPoints;
        uint8_t m_ShieldPoints;
        uint8_t m_Energy;
        uint32_t m_ResourceAmount;
        uint16_t m_HangarCount;
        uint16_t m_Flags;
        uint32_t m_Unused;
    };

    class CHKCuwpChunk : public IChunk
    {
        public:
        CHKCuwpChunk(const std::vector<char>& data, const std::string& type) : IChunk(type)
        {
            SetBytes(data);
        }

        CUWPSlot* GetSlot(unsigned int index)
        {
            if (index >= 64)
            {
                return nullptr;
            }

            auto& bytes = GetBytes();
            return (CUWPSlot*)(bytes.data() + index * sizeof(CUWPSlot));
        }

        void SetBytes(const std::vector<char>& data);
    };

}

#endif
