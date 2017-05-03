#ifndef __LANGUMS_PROCESS_UTIL
#define __LANGUMS_PROCESS_UTIL

#include <string>
#include <vector>

namespace Langums
{

    namespace Process
    {

        typedef void* ProcessHandle;

        ProcessHandle OpenByName(const std::string& processName, void*& baseAddress);
        bool Close(ProcessHandle process);
        bool ReadMemory(ProcessHandle process, void* address, size_t size, void* dstBuffer, size_t& retBytesRead);

    }
    
}

#endif
