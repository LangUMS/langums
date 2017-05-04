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
        CompilerException(const char* error, IIRInstruction* instruction) : m_Instruction(instruction), std::exception(error) {}
        CompilerException(const std::string& error, IIRInstruction* instruction) : m_Instruction(instruction), std::exception(error.c_str()) {}

        IIRInstruction* GetInstruction() const
        {
            return m_Instruction;
        }

        private:
        IIRInstruction* m_Instruction = nullptr;
    };

    class Compiler
    {
        public:
        Compiler(bool debug);

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

        unsigned int CodeGen_CopyReg(unsigned int dstRegId, unsigned int srcRegId, unsigned int& nextAddress, unsigned int retAddress, IIRInstruction* instruction);
        void Action_PreserveTrigger(CHK::TriggerAction& retAction);
        void Action_Wait(unsigned int milliseconds, CHK::TriggerAction& retAction);
        void Action_JumpTo(unsigned int address, CHK::TriggerAction& retAction);

        void DoIndirectJump(TriggerBuilder& trigger);
        void EmitIndirectJumpCode(unsigned int& nextAddress);
        void EmitMulInstructionCode(unsigned int& nextAddress);

        unsigned int GetLocationIdByName(const std::string& name, IIRInstruction* instruction);

        int GetLastTriggerActionId(const CHK::Trigger& trigger);

        void PushTriggers(const std::vector<CHK::Trigger>& triggers, IIRInstruction* jmpTarget = nullptr)
        {
            m_Triggers.reserve(m_Triggers.size() + triggers.size());

            for (auto& trigger : triggers)
            {
                m_Triggers.push_back(trigger);
                if (jmpTarget != nullptr)
                {
                    auto ptr = &m_Triggers.back();
                    m_JmpPatchups.insert(std::make_pair(ptr, jmpTarget));
                }
            }
        }

        std::vector<CHK::Trigger> m_Triggers;
        std::unordered_map<Trigger*, IIRInstruction*> m_JmpPatchups;
        std::set<IIRInstruction*> m_JumpTargets;
        std::unordered_map<IIRInstruction*, unsigned int> m_JumpAddresses;

        uint32_t m_CopyBatchSize = 8192u;
        uint32_t m_HyperTriggerCount = 5;
        uint8_t m_TriggersOwner = 1;

        CHK::File* m_File = nullptr;
        CHK::CHKStringsChunk* m_StringsChunk = nullptr;
        CHK::CHKLocationsChunk* m_LocationsChunk = nullptr;
        CHK::CHKCuwpChunk* m_CuwpChunk = nullptr;
        CHK::CHKCuwpUsedChunk* m_CuwpUsedChunk = nullptr;

        unsigned int m_StackPointer;
        unsigned int m_MultiplyAddress;

        std::vector<RegisterDef> m_RegisterMap;
        bool m_Debug = false;
    };

}

#endif
