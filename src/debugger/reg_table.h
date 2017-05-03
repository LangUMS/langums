#ifndef __LANGUMS_REGTABLE_H
#define __LANGUMS_REGTABLE_H

#include "process_util.h"

#define PLAYER_COUNT 12
#define UNIT_COUNT 228 

#define DEATH_TABLE_OFFSET 0x01713324

namespace Langums
{

    class RegTable
    {
        public:
        RegTable(Process::ProcessHandle gameHandle, void* baseAddress) :
            m_GameHandle(gameHandle), m_BaseAddress(baseAddress)
        {
            m_RegisterTableAddress = (char*)m_BaseAddress + DEATH_TABLE_OFFSET;
        }

        bool Update()
        {
            using namespace Process;

            auto byteSize = sizeof(unsigned int) * PLAYER_COUNT * UNIT_COUNT;
            size_t bytesRead;
            if (!ReadMemory(m_GameHandle, m_RegisterTableAddress, byteSize, m_Registers, bytesRead))
            {
                return false;
            }

            if (bytesRead != byteSize)
            {
                return false;
            }
            
            return true;
        }

        private:
        unsigned int m_Registers[UNIT_COUNT][PLAYER_COUNT];
        Process::ProcessHandle m_GameHandle;
        void* m_BaseAddress = nullptr;
        void* m_RegisterTableAddress;
    };

}

#endif
