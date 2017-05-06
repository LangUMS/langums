#ifndef __LANGUMS_VSCODE_PROTOCOL_H
#define __LANGUMS_VSCODE_PROTOCOL_H

#include <string>
#include <vector>
#include <unordered_map>

#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"

namespace Langums
{

    using namespace rapidjson;

    namespace DebuggerProtocol
    {

        class IDebuggerEvent
        {
            public:
            IDebuggerEvent(const std::string& type) : m_Type(type)
            {}

            void Serialize(Writer<StringBuffer>& writer)
            {
                writer.StartObject();
                writer.Key("type");
                writer.String(m_Type.c_str());

                writer.Key("data");
                writer.StartObject();
                SerializeData(writer);
                writer.EndObject();

                writer.EndObject();
            }

            virtual void SerializeData(Writer<StringBuffer>& writer)
            {}

            private:
            std::string m_Type;
        };

        class Initialized : public IDebuggerEvent
        {
            public:
            Initialized() : IDebuggerEvent("initialized")
            {}
        };

        class LogMessage : public IDebuggerEvent
        {
            public:
            LogMessage(const std::string& message) : m_Message(message), IDebuggerEvent("log")
            {}

            void SerializeData(Writer<StringBuffer>& writer)
            {
                writer.Key("message");
                writer.String(m_Message.c_str());
            }

            private:
            std::string m_Message;
        };

        class BreakpointsSet : public IDebuggerEvent
        {
            public:
            BreakpointsSet() : IDebuggerEvent("breakpoints-set")
            {}

            void AddBreakpoint(unsigned int line, bool valid)
            {
                m_Breakpoints.push_back(std::make_pair(line, valid));
            }

            void SerializeData(Writer<StringBuffer>& writer)
            {
                writer.Key("breakpoints");
                writer.StartArray();

                for (auto& pair : m_Breakpoints)
                {
                    writer.StartObject();
                    writer.Key("line");
                    writer.Int(pair.first);

                    writer.Key("valid");
                    writer.Bool(pair.second);

                    writer.EndObject();
                }

                writer.EndArray();
            }

            private:
            std::vector<std::pair<unsigned int, bool>> m_Breakpoints;
        };

        class BreakpointHit : public IDebuggerEvent
        {
            public:
            BreakpointHit() : IDebuggerEvent("breakpoint-hit")
            {}

            void SerializeData(Writer<StringBuffer>& writer)
            {
            }
        };

        struct StackFrame
        {
            int m_Id;
            int m_LineNumber;
            std::string m_Name;
            std::vector<std::pair<std::string, std::string>> m_Variables;
        };

        class StackTrace : public IDebuggerEvent
        {
            public:
            StackTrace(const std::vector<StackFrame>& stackFrames) : 
                m_StackFrames(stackFrames), IDebuggerEvent("stacktrace")
            {}

            void SerializeData(Writer<StringBuffer>& writer)
            {
                writer.Key("frames");

                writer.StartArray();
                for (auto i = (int)m_StackFrames.size() - 1; i >= 0; i--)
                {
                    auto& frame = m_StackFrames[i];
                    writer.StartObject();

                    writer.Key("id");
                    writer.Int(frame.m_Id);

                    writer.Key("name");
                    writer.String(frame.m_Name.c_str());

                    writer.Key("line");
                    writer.Int(frame.m_LineNumber);

                    writer.Key("variables");

                    writer.StartArray();
                    for (auto& variable : frame.m_Variables)
                    {
                        writer.StartObject();

                        writer.Key("name");
                        writer.String(variable.first.c_str());

                        writer.Key("value");
                        writer.String(variable.second.c_str());

                        writer.EndObject();
                    }
                    writer.EndArray();

                    writer.EndObject();
                }
                writer.EndArray();
            }

            private:
            std::vector<StackFrame> m_StackFrames;
        };

    }

}

#endif
