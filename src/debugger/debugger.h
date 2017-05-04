#ifndef __LANGUMS_DEBUGGER_H
#define __LANGUMS_DEBUGGER_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>

#include "../compiler/ir.h"
#include "debugger_thread.h"

namespace Langums
{

    class Debugger
    {
        public:
        Debugger(const std::vector<std::unique_ptr<IIRInstruction>>& instructions, const std::string& source);

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
        void RemoveBreakpoint(unsigned int breakpointId);
        int GetCurrentBreakpointId() const;
        void ContinueToNextAddress();

        const std::string& GetSource() const
        {
            return m_Source;
        }

        private:
        IIRInstruction* AddressToInstruction(unsigned int address) const;

        // read only
        std::vector<IIRInstruction*> m_Instructions;
        std::string m_Source;
        std::string m_ProcessName;

        std::unique_ptr<DebuggerThread> m_DebuggerThread;

    };

}

#endif
