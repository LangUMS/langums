#ifndef __LANGUMS_COMPILER_H
#define __LANGUMS_COMPILER_H

#include <memory>
#include <unordered_map>
#include <set>

#include "ir.h"
#include "../libchk/chk.h"
#include "triggerbuilder.h"

#define MAX_TRIGGERS_COUNT 16384

namespace Langums
{

    class CompilerException : public std::exception
    {
        public:
        CompilerException(const char* error) : std::exception(error) {}
        CompilerException(const std::string& error) : std::exception(error.c_str()) {}
    };

    class Compiler
    {
        public:
        Compiler();

        void SetCopyBatchSize(uint32_t copyBatchSize)
        {
            m_CopyBatchSize = copyBatchSize;
        }

        void SetTriggersOwner(uint8_t owner)
        {
            m_TriggersOwner = owner;
        }

        void SetCustomRegisterDefinitions(const std::vector<RegisterDef>& defs)
        {
            g_RegisterMap = defs;
        }

        bool Compile(const std::vector<std::unique_ptr<IIRInstruction>>& instructions, CHK::File& chk, bool preserveTriggers);

        private:
        void Cond_Always(CHK::TriggerCondition& retCondition);

        unsigned int CodeGen_CopyReg(unsigned int dstRegId, unsigned int srcRegId, unsigned int& nextAddress, unsigned int retAddress);
        void Action_PreserveTrigger(CHK::TriggerAction& retAction);
        void Action_Wait(unsigned int milliseconds, CHK::TriggerAction& retAction);
        void Action_JumpTo(unsigned int address, CHK::TriggerAction& retAction);

        unsigned int GetLocationIdByName(const std::string& name);

        int GetLastTriggerActionId(const CHK::Trigger& trigger);
        uint32_t m_CopyBatchSize = 65536u;
        uint32_t m_HyperTriggerCount = 5;
        uint8_t m_TriggersOwner = 1;

        CHK::File* m_File = nullptr;
        CHK::CHKStringsChunk* m_StringsChunk = nullptr;
        CHK::CHKLocationsChunk* m_LocationsChunk = nullptr;
        CHK::CHKCuwpChunk* m_CuwpChunk = nullptr;
        CHK::CHKCuwpUsedChunk* m_CuwpUsedChunk = nullptr;

        std::vector<CHK::Trigger> m_Triggers;
        std::set<IIRInstruction*> m_JumpTargets;
        std::unordered_map<IIRInstruction*, unsigned int> m_JumpAddresses;

        unsigned int m_StackPointer = MAX_REGISTER_INDEX;
        bool m_DebugTrace = true;

        std::vector<RegisterDef> m_RegisterMap;
    };

}

#endif
