#ifndef __LANGUMS_DEBUGGER_H
#define __LANGUMS_DEBUGGER_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <set>

#include "../compiler/ir.h"
#include "../ast/ast.h"
#include "debugger_thread.h"

namespace LangUMS
{

    class Debugger
    {
        public:
        Debugger(const std::vector<std::unique_ptr<IIRInstruction>>& instructions, const std::string& source, const std::shared_ptr<IASTNode>& ast);

        DebuggerThreadState GetState() const;
        
        bool Attach(const std::string& processName);
        void Detach();
        
        unsigned int GetCurrentAddress() const;
        IIRInstruction* GetCurrentInstruction() const;
        IASTNode* GetCurrentASTNode() const;
        int GetCurrentSourceCharIndex() const;
        
        unsigned int GetRegisterValue(unsigned int regId) const;
        void SetRegisterValue(unsigned int regId, unsigned int value);

        void SuspendExecution();
        void ResumeExecution();
        
        void SetBreakpoint(unsigned int breakpointId, unsigned int address);
        bool SetBreakpointBySourceLine(unsigned int breakpointId, unsigned int lineNumber);
        void RemoveBreakpoint(unsigned int breakpointId);
        void RemoveAllBreakpoints();
        
        int GetCurrentBreakpointId() const;
        void ContinueToNextAddress();

        const std::string& GetSource() const
        {
            return m_Source;
        }

        int CharIndexToLineNumber(int charIndex)
        {
            for (auto i = 0u; i < m_LineStartIndices.size(); i++)
            {
                if (m_LineStartIndices[i] > charIndex)
                {
                    return i;
                }
            }

            return 0;
        }

        private:
        void GatherASTNodeCharIndices(IASTNode* node);

        IIRInstruction* AddressToInstruction(unsigned int address) const;
        std::vector<IIRInstruction*> SourceLineToInstructions(unsigned int lineNumber);
        int InstructionToAddress(IIRInstruction* instruction);

        // read only
        std::vector<IIRInstruction*> m_Instructions;
        std::string m_Source;
        std::vector<std::string> m_SourceLines;
        std::vector<int> m_LineStartIndices;

        std::string m_ProcessName;

        std::unique_ptr<DebuggerThread> m_DebuggerThread;
        IASTNode* m_AST;

        std::unordered_map<IASTNode*, int> m_ASTNodeCharIndices;

        std::unordered_map<unsigned int, std::vector<unsigned int>> m_Breakpoints;
        std::unordered_map<unsigned int, unsigned int> m_BreakpointIdToAddress;

        unsigned int m_NextBreakpointId = 1;
    };

}

#endif
