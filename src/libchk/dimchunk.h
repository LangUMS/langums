#ifndef __LIBCHK_DIMCHUNK_H
#define __LIBCHK_DIMCHUNK_H

#include <unordered_map>
#include <string>

#include "ichunk.h"
#include "../serialization.h"

namespace CHK
{

    class CHKDimChunk : public IChunk
    {
        public:
        CHKDimChunk(const std::vector<char>& data, const std::string& type) : IChunk(type)
        {
            SetBytes(data);
        }

        uint16_t GetWidth() const
        {
            return m_Width;
        }

        uint16_t GetHeight() const
        {
            return m_Height;
        }

        void SetBytes(const std::vector<char>& data);

        private:
        uint16_t m_Width = 0;
        uint16_t m_Height = 0;
    };

}

#endif
