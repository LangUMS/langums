#ifndef __LANGUMS_MEMSEARCH_H
#define __LANGUMS_MEMSEARCH_H

#include "process_util.h"

namespace Langums
{
    using namespace Process;

    void* FindAddressOf32BitValue(ProcessHandle handle, unsigned int value, void* startAddress)
    {
        auto address = (unsigned int*)startAddress;
        std::vector<unsigned int> chunk;
        chunk.resize(1024);

        size_t bytesRead;
        while (ReadMemory(handle, address, chunk.size() * sizeof(unsigned int), chunk.data(), bytesRead))
        {
            for (auto i = 0u; i < chunk.size(); i++)
            {
                if (chunk[i] == value)
                {
                    return address + i;
                }
            }

            address += chunk.size();
        }

        return nullptr;
    }

    void* FindAddressOfString(ProcessHandle handle, const std::string& s, void* startAddress)
    {
        auto address = (char*)startAddress;
        auto len = s.length();
        auto windowSize = len * 4;

        std::vector<char> chunk;
        chunk.resize(len);

        size_t bytesRead;
        while (ReadMemory(handle, address, chunk.size(), chunk.data(), bytesRead))
        {
            for (auto q = 0u; q < len; q++)
            {
                if (chunk[q] != s[q])
                {
                    break;
                }

                if (q == len - 1)
                {
                    return address;
                }
            }

            address += 1;
        }

        return nullptr;
    }

}

#endif
