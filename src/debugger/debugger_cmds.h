#ifndef __LANGUMS_DEBUGGER_CMDS_H
#define __LANGUMS_DEBUGGER_CMDS_H

namespace LangUMS
{

    enum class DebuggerCmdType
    {
        Shutdown = 0,
        Suspend,
        Resume,
        SetRegister,
        SetBreakpoint,
        RemoveBreakpoint,
        ContinueToNext
    };

    class IDebuggerCmd
    {
        public:
        IDebuggerCmd(DebuggerCmdType type) : m_Type(type)
        {}

        virtual ~IDebuggerCmd()
        {}

        DebuggerCmdType GetType() const
        {
            return m_Type;
        }

        private:
        DebuggerCmdType m_Type;
    };

    class DebuggerShutdownCmd : public IDebuggerCmd
    {
        public:
        DebuggerShutdownCmd() : IDebuggerCmd(DebuggerCmdType::Shutdown)
        {}
    };

    class DebuggerSuspendCmd : public IDebuggerCmd
    {
        public:
        DebuggerSuspendCmd() : IDebuggerCmd(DebuggerCmdType::Suspend)
        {}
    };

    class DebuggerResumeCmd : public IDebuggerCmd
    {
        public:
        DebuggerResumeCmd() : IDebuggerCmd(DebuggerCmdType::Resume)
        {}
    };

    class DebuggerSetRegisterCmd : public IDebuggerCmd
    {
        public:
        DebuggerSetRegisterCmd(unsigned int regId, unsigned int value) :
            m_RegisterId(regId), m_Value(value), IDebuggerCmd(DebuggerCmdType::SetRegister)
        {}

        unsigned int GetRegisterId() const
        {
            return m_RegisterId;
        }

        unsigned int GetValue() const
        {
            return m_Value;
        }

        private:
        unsigned int m_RegisterId;
        unsigned int m_Value;
    };

    class DebuggerSetBreakpointCmd : public IDebuggerCmd
    {
        public:
        DebuggerSetBreakpointCmd(unsigned int breakpointId, unsigned int address) :
            m_Id(breakpointId), m_Address(address), IDebuggerCmd(DebuggerCmdType::SetBreakpoint)
        {}

        unsigned int GetId() const
        {
            return m_Id;
        }

        unsigned int GetAddress() const
        {
            return m_Address;
        }

        private:
        unsigned int m_Id;
        unsigned int m_Address;
    };

    class DebuggerRemoveBreakpointCmd : public IDebuggerCmd
    {
        public:
        DebuggerRemoveBreakpointCmd(unsigned int breakpointId) :
            m_Id(breakpointId), IDebuggerCmd(DebuggerCmdType::RemoveBreakpoint)
        {}

        unsigned int GetId() const
        {
            return m_Id;
        }

        private:
        unsigned int m_Id;
    };

    class DebuggerContinueToNextCmd : public IDebuggerCmd
    {
        public:
        DebuggerContinueToNextCmd() : IDebuggerCmd(DebuggerCmdType::ContinueToNext)
        {}
    };

}

#endif
