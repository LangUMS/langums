#ifndef __LANGUMS_SERIALIZATION_H
#define __LANGUMS_SERIALIZATION_H

#include <vector>

class BinaryWriter
{
    public:
    template <typename T>
    void Write(const T& value)
    {
        m_Buffer.resize(m_Buffer.size() + sizeof(T));
        memcpy(m_Buffer.data() + m_Buffer.size() - sizeof(T), &value, sizeof(T));
    }

    template <typename T>
    void WriteVector(const std::vector<T>& values)
    {
        auto size = sizeof(T) * values.size();
        m_Buffer.resize(m_Buffer.size() + size);
        memcpy(m_Buffer.data() + m_Buffer.size() - size, values.data(), size);
    }

    void Write(const char* ptr, size_t size)
    {
        m_Buffer.resize(m_Buffer.size() + size);
        memcpy(m_Buffer.data() + m_Buffer.size() - size, ptr, size);
    }

    const std::vector<char>& GetBuffer() const
    {
        return m_Buffer;
    }

    private:
    std::vector<char> m_Buffer;
};

#endif
