#ifndef __LIBCHK_ICHUNK_H
#define __LIBCHK_ICHUNK_H

#include <vector>

namespace CHK
{

    class IChunk
    {
        public:
        IChunk(const std::string& type) : m_Type(type) {}
        IChunk(const std::vector<char>& data, const std::string& type) : m_Data(data), m_Type(type) {}

        virtual ~IChunk() {};

        const std::string& GetType() const
        {
            return m_Type;
        }

        const std::vector<char>& GetBytes() const
        {
            return m_Data;
        }

        virtual void SetBytes(const std::vector<char>& data)
        {
            m_Data = data;
        }

        private:
        std::string m_Type = "UNKN";
        std::vector<char> m_Data;
    };

}

#endif
