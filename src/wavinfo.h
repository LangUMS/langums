#ifndef __LANGUMS_WAVINFO_H
#define __LANGUMS_WAVINFO_H

#include <string>

namespace LangUMS
{

    class WAVInfo
    {
        public:
        bool ReadInfo(const std::string& path);

        unsigned int GetLengthMilliseconds() const
        {
            return m_LengthMilliseconds;
        }

        const std::vector<char>& GetData() const
        {
            return m_Data;
        }

        private:
        unsigned int m_LengthMilliseconds;
        std::vector<char> m_Data;
    };

}

#endif
