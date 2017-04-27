#include "stringschunk.h"

namespace CHK
{

    const char* CHKStringsChunk::GetString(size_t index) const
    {
        if (index >= m_Offsets.size())
        {
            return nullptr;
        }

        auto& bytes = GetBytes();
        if (m_Offsets[index] >= bytes.size())
        {
            return nullptr;
        }

        return bytes.data() + m_Offsets[index];
    }

    int CHKStringsChunk::FindString(const std::string& s)
    {
        auto it = m_Indices.find(s);
        if (it == m_Indices.end())
        {
            return -1;
        }

        return (*it).second;
    }

    size_t CHKStringsChunk::InsertString(const std::string& s)
    {
        auto it = m_Indices.find(s);
        if (it != m_Indices.end())
        {
            return (*it).second;
        }

        auto& bytes = GetBytes();
        auto headerSize = sizeof(uint16_t) * (1 + m_Offsets.size());

        auto index = m_Strings.size() + 1;

        BinaryWriter writer;
        writer.Write<uint16_t>((uint16_t)m_Offsets.size());
        m_Offsets[index] = (uint16_t)bytes.size();

        writer.WriteVector(m_Offsets);
        writer.Write(bytes.data() + headerSize, bytes.size() - headerSize);
        writer.Write(s.data(), s.length());
        writer.Write<char>(0);

        SetBytes(writer.GetBuffer());
        return index;
    }

    void CHKStringsChunk::SetBytes(const std::vector<char>& data)
    {
        m_Offsets.clear();
        m_Indices.clear();

        IChunk::SetBytes(data);

        auto numStrings = *((uint16_t*)data.data());

        auto i = 2;
        m_Offsets.reserve(numStrings);

        for (auto q = 0; q < numStrings; q++)
        {
            auto offset = *((uint16_t*)(data.data() + i));
            i += 2;

            m_Offsets.push_back(offset);
        }

        for (auto i = 0u; i < m_Offsets.size(); i++)
        {
            auto str = GetString(i);

            if (str != nullptr && strlen(str) > 0)
            {
                m_Indices[str] = i;
                m_Strings.push_back(str);
            }
        }
    }

}