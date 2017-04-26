#ifndef __LIBCHK_VERCHUNK_H
#define __LIBCHK_VERCHUNK_H

#include "ichunk.h"

namespace CHK
{

    class CHKVerChunk : public IChunk
    {
        public:
        CHKVerChunk(const std::vector<char>& data, const std::string& type) : IChunk(type)
        {
            SetBytes(data);
        }

        uint16_t GetVersion() const
        {
            return m_Version;
        }

        virtual void SetBytes(const std::vector<char>& data)
        {
            m_Version = *((uint16_t*)data.data());
            IChunk::SetBytes(data);
        }

        private:
        uint16_t m_Version = 0;
    };

}

#endif
