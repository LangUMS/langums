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

}

#endif
