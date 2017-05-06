#ifndef __LANGUMS_IR_H
#define __LANGUMS_IR_H

#include <memory>
#include <unordered_map>
#include <set>
#include <algorithm>

#include "../ast/ast.h"
#include "../libchk/src/chk.h"
#include "../stringutil.h"

#define MAX_EVENT_CONDITIONS 63
#define JMP_TO_END_OFFSET_CONSTANT 0xB4DF00D

#include "ir_constants.h"
#include "ir_instructions.h"
#include "ir_exceptions.h"
#include "reg_aliases.h"

namespace Langums
{

    using namespace CHK;

    class IRCompiler
    {
        public:
        bool Compile(const std::shared_ptr<IASTNode>& ast);
        void Optimize();

        const std::vector<std::unique_ptr<IIRInstruction>>& GetInstructions() const
        {
            return m_Instructions;
        }

        std::string DumpInstructions(bool lineNumbers) const;

        const std::set<std::string>& GetWavFilenames() const
        {
            return m_WavFilenames;
        }

        private:
        void EmitInstruction(IIRInstruction* instruction, std::vector<std::unique_ptr<IIRInstruction>>& instructions, IASTNode* node, RegisterAliases& aliases);

        void EmitFunctionCall(ASTFunctionCall* fnCall, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool ignoreReturnValue);
        void EmitBinaryExpression(ASTBinaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        void EmitUnaryExpression(ASTUnaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        void EmitExpression(IASTNode* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        unsigned int EmitBlockStatement(ASTBlockStatement* blockStatement, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);
        unsigned int EmitFunction(ASTFunctionDeclaration* fn, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases);

        bool IsRegisterName(const std::string& name, RegisterAliases& aliases, IASTNode* node) const;
        int RegisterNameToIndex(const std::string& name, unsigned int arrayIndex, RegisterAliases& aliases, IASTNode* node) const;

        int UnitNameToId(const std::string& name) const;
        int PlayerNameToId(const std::string& name) const;

        unsigned int ParseArrayExpression(const std::shared_ptr<IASTNode>& expression);
        uint8_t ParsePlayerIdArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        ConditionComparison ParseComparisonArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        int ParseQuantityArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        uint8_t ParseUnitTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        uint32_t ParseAIScriptArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        std::string ParseLocationArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        ResourceType ParseResourceTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        ScoreType ParseScoreTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        LeaderboardType ParseLeaderboardType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        AllianceStatus ParseAllianceStatusArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        EndGameType ParseEndGameCondition(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        TriggerActionState ParseOrderType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        TriggerActionState ParseToggleState(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        ModifyType ParseModifyType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex);
        UnitPropType ParseUnitPropType(const std::string& propName, IASTNode* node);

        int ParseQuantityExpression(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex,
            std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool& isLiteral);

        std::vector<std::unique_ptr<IIRInstruction>> m_Instructions;
        std::unordered_map<std::string, ASTFunctionDeclaration*> m_FunctionDeclarations;
        std::unordered_map<ASTFunctionDeclaration*, unsigned int> m_FunctionIndices;
        std::unordered_map<std::string, unsigned int> m_UnitProperties;

        RegisterAliases m_GlobalAliases;

        unsigned int m_EventCount = 0;
        std::set<std::string> m_WavFilenames;

        std::vector<std::shared_ptr<StackFrame>> m_DebugStackFrames;
        IASTNode* m_Unit;
    };

}

#endif
