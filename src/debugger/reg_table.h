#ifndef __LANGUMS_REGTABLE_H
#define __LANGUMS_REGTABLE_H

#include "process_util.h"

#define PLAYER_COUNT 12
#define UNIT_COUNT 228 


namespace LangUMS
{

    class RegTable
    {
        public:
        RegTable(Process::ProcessHandle gameHandle, void* address) :
            m_GameHandle(gameHandle), m_RegisterTableAddress(address)
        {
        }

        unsigned int Get(unsigned int playerId, unsigned int unitId) const
        {
            return m_Registers[unitId][playerId];
        }

        bool Set(unsigned int playerId, unsigned int unitId, unsigned int value)
        {
            using namespace Process;

            size_t bytesWritten;
            if (!WriteMemory(m_GameHandle, &m_Registers[unitId][playerId], sizeof(unsigned int), &value, bytesWritten))
            {
                return false;
            }

            if (bytesWritten != sizeof(unsigned int))
            {
                return false;
            }

            m_Registers[unitId][playerId] = value;
            return true;
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
