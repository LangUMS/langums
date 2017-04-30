#ifndef __LANGUMS_IR_H
#define __LANGUMS_IR_H

#include <memory>
#include <unordered_map>
#include <set>
#include <algorithm>

#include "../ast/ast.h"
#include "../libchk/chk.h"
#include "../stringutil.h"

#define MAX_EVENT_CONDITIONS 63
#define JMP_TO_END_OFFSET_CONSTANT 0xB4DF00D

#include "ir_constants.h"
#include "ir_instructions.h"

namespace Langums
{

    using namespace CHK;

    class IRCompilerException : public std::exception
    {
        public:
        IRCompilerException(const char* error) : std::exception(error) {}
        IRCompilerException(const std::string& error) : std::exception(error.c_str()) {}
    };

    class RegisterAliases
    {
        public:
        RegisterAliases() {}

        bool HasAlias(const std::string& name, unsigned int index) const
        {
            auto it = m_Aliases.find(name);
            if (it == m_Aliases.end())
            {
                return false;
            }

            auto& registers = (*it).second;
            if (index >= registers.size())
            {
                if (registers.size() == 1)
                {
                    return false;
                }
                else
                {
                    return false;
                }
            }

            return true;
        }

        int GetAlias(const std::string& name, unsigned int index) const
        {
            auto it = m_Aliases.find(name);
            if (it == m_Aliases.end())
            {
                throw IRCompilerException(SafePrintf("Invalid register name \"%\"", name));
            }

            auto& registers = (*it).second;
            if (index >= registers.size())
            {
                if (registers.size() == 1)
                {
                    throw IRCompilerException(SafePrintf("Invalid register name \"%\"", name));
                }
                else
                {
                    throw IRCompilerException(SafePrintf("Array access out of bounds for \"%[%]\"", name, index));
                }
            }

            return Reg_ReservedEnd + registers[index];
        }

        void Allocate(const std::string& name, unsigned int count)
        {
            auto& registers = m_Aliases[name];

            for (auto& id : registers)
            {
                m_FreeIds.push_back(id);
            }

            registers.clear();

            for (auto i = 0u; i < count; i++)
            {
                registers.push_back(GetNextFreeId());
            }
        }

        void Deallocate(const std::string& name)
        {
            auto& registers = m_Aliases[name];
            for (auto id : registers)
            {
                m_FreeIds.push_back(id);
            }

            m_Aliases.erase(name);
        }

        private:
        unsigned int GetNextFreeId()
        {
            if (m_FreeIds.size() > 0)
            {
                auto next = m_FreeIds.back();
                m_FreeIds.pop_back();
                return next;
            }

            return m_NextFreeId++;
        }

        std::unordered_map<std::string, std::vector<unsigned int>> m_Aliases;
        std::vector<unsigned int> m_FreeIds;
        unsigned int m_NextFreeId = 0;
    };

    class IRCompiler
    {
        public:
        bool Compile(const std::shared_ptr<IASTNode>& ast);
        void Optimize();

        const std::vector<std::unique_ptr<IIRInstruction>>& GetInstructions() const
        {
            return m_Instructions;
        }

        std::string DumpInstructions(bool lineNumbers) const
        {
            std::string dump;

            for (auto i = 0u; i < m_Instructions.size(); i++)
            {
                if (lineNumbers)
                {
                    dump += SafePrintf("%. ", i);
                }

                dump += m_Instructions[i]->DebugDump();

                if (i < m_Instructions.size() - 1)
                {
                    dump += '\n';
                }
            }

            return dump;
        }

        const std::set<std::string>& GetWavFilenames() const
        {
            return m_WavFilenames;
        }

        private:
        void EmitInstruction(IIRInstruction* instruction, std::vector<std::unique_ptr<IIRInstruction>>& instructions)
        {
            instructions.push_back(std::unique_ptr<IIRInstruction>(instruction));
        }

        void EmitFunctionCall(ASTFunctionCall* fnCall, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool ignoreReturnValue);
        void EmitBinaryExpression(ASTBinaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        void EmitUnaryExpression(ASTUnaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        void EmitExpression(IASTNode* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        unsigned int EmitBlockStatement(ASTBlockStatement* blockStatement, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        unsigned int EmitFunction(ASTFunctionDeclaration* fn, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);

        bool IsRegisterName(const std::string& name, RegisterAliases& aliases) const;
        int RegisterNameToIndex(const std::string& name, unsigned int arrayIndex, RegisterAliases& aliases) const;

        int UnitNameToId(const std::string& name) const;
        int PlayerNameToId(const std::string& name) const;

        uint8_t ParsePlayerIdArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        ConditionComparison ParseComparisonArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        int ParseQuantityArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        uint8_t ParseUnitTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        uint32_t ParseAIScriptArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        std::string ParseLocationArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        CHK::ResourceType ParseResourceTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        CHK::ScoreType ParseScoreTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        LeaderboardType ParseLeaderboardType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        AllianceStatus ParseAllianceStatusArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        EndGameType ParseEndGameCondition(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        CHK::TriggerActionState ParseOrderType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        CHK::TriggerActionState ParseToggleState(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        ModifyType ParseModifyType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        UnitPropType ParseUnitPropType(const std::string& propName);

        int ParseQuantityExpression(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex,
            std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool& isLiteral);

        std::vector<std::unique_ptr<IIRInstruction>> m_Instructions;
        std::unordered_map<std::string, ASTFunctionDeclaration*> m_FunctionDeclarations;
        std::unordered_map<ASTFunctionDeclaration*, unsigned int> m_FunctionIndices;

        std::unordered_map<std::string, unsigned int> m_UnitProperties;

        RegisterAliases m_GlobalAliases;

        std::vector<std::unique_ptr<IIRInstruction>> m_PollEventsInstructions;

        unsigned int m_EventCount = 0;
        std::set<std::string> m_WavFilenames;
    };

}

#endif
