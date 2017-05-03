#ifndef __LANGUMS_MEMWATCH_H
#define __LANGUMS_MEMWATCH_H

#include <vector>
#include <sstream>

#include "process_util.h"
#include "../log.h"

namespace Langums
{

    struct MemorySnapshot
    {
        void* m_StartAddress;
        size_t m_Size;
        std::vector<unsigned int> m_Bytes;  
    };

    bool TakeSnapshot(Process::ProcessHandle handle, void* startAddress, size_t size, MemorySnapshot& retSnapshot)
    {
        using namespace Process;

        retSnapshot.m_StartAddress = startAddress;
        retSnapshot.m_Size = size;
        retSnapshot.m_Bytes.resize(size / sizeof(unsigned int));

        size_t bytesRead;
        if (!ReadMemory(handle, startAddress, size, retSnapshot.m_Bytes.data(), bytesRead))
        {
            return false;
        }

        if (bytesRead != size)
        {
            return false;
        }

        return true;
    }

    struct SnapshotDifference
    {
        void* m_StartAddress;
        unsigned int m_OldValue;
        unsigned int m_NewValue;
    };

    std::vector<SnapshotDifference> GatherSnapshotDifferences(const MemorySnapshot& a, const MemorySnapshot& b)
    {
        std::vector<SnapshotDifference> differences;

        if (a.m_StartAddress != b.m_StartAddress)
        {
            LOG_F("Trying to compare snapshots of different addresses.");
            return differences;
        }

        if (a.m_Size != b.m_Size)
        {
            LOG_F("Trying to compare snapshots of different sizes");
            return differences;
        }

        auto address = (char*)a.m_StartAddress;

        for (auto i = 0u; i < a.m_Size / sizeof(unsigned int); i++)
        {
            if (a.m_Bytes[i] != b.m_Bytes[i])
            {
                SnapshotDifference diff;
                diff.m_StartAddress = address;
                diff.m_OldValue = (unsigned int)b.m_Bytes[i];
                diff.m_NewValue = (unsigned int)a.m_Bytes[i];
                differences.push_back(diff);
            }

            address++;
        }

        return differences;
    }

    void PrintSnapshotDifferences(const MemorySnapshot& control, const MemorySnapshot& a, const MemorySnapshot& b)
    {
        auto ignoreDiffs = GatherSnapshotDifferences(control, a);
        auto diffs = GatherSnapshotDifferences(a, b);
        
        std::vector<SnapshotDifference> outDiffs;

        for (auto& diff : diffs)
        {
            auto ignore = false;

            for (auto& diff2 : ignoreDiffs)
            {
                if (diff.m_StartAddress == diff2.m_StartAddress)
                {
                    ignore = true;
                    break;
                }
            }

            if (ignore)
            {
                continue;
            }

            outDiffs.push_back(diff);
        }

        
        for (auto& diff : outDiffs)
        {
            std::stringstream ss;
            ss << std::hex << (size_t)diff.m_StartAddress;
            LOG_F("@ 0x% - was: %, now: %", ss.str(), diff.m_OldValue, diff.m_NewValue);
        }
    }

}

#endif
