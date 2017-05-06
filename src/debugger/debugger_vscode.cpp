#include <string>
#include <iostream>

#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"

#include "../log.h"
#include "../pretty_errors.h"

#include "debugger_vscode.h"

namespace Langums
{

    DebuggerVSCode::~DebuggerVSCode()
    {
        if (m_EventsThread == nullptr)
        {
            return;
        }

        m_ShutdownEventsThread = true;
        while (m_ShutdownEventsThread)
        {
        }

        m_EventsThread->join();
    }

    bool DebuggerVSCode::Run()
    {
        LOG_F("Starting VS code debug session.");
        
        m_SourceLines = Split(m_Debugger->GetSource());

        while (m_Debugger->GetState() != DebuggerThreadState::Attached)
        {
        }

        m_EventsThread = std::make_unique<std::thread>(std::bind(&DebuggerVSCode::RunEventsThread, this));

        auto initialized = std::make_unique<DebuggerProtocol::Initialized>();
        SendEvent(initialized.get());

        std::string buffer;

        std::cin >> std::noskipws;

        while (true)
        {
            char c;
            std::cin >> c;

            if (c == '\n')
            {
                ProcessCommand(buffer);
                buffer.clear();
            }
            else
            {
                buffer.push_back(c);
            }
        }

        return true;
    }

    void DebuggerVSCode::ProcessCommand(const std::string& json)
    {
        if (json.length() == 0)
        {
            return;
        }

        using namespace rapidjson;
        
        Document doc;
        doc.Parse(json.c_str());

        if (!doc.IsObject())
        {
            LOG_F("Error: Malformed command \"%\"", json);
            return;
        }

        if (!doc.HasMember("type"))
        {
            LOG_F("Error: Malformed command \"%\", expected \"type\" property", json);
            return;
        }

        auto& type = doc["type"];
        auto typeString = std::string(type.GetString());

        if (typeString == "set-breakpoints")
        {
            auto& breakpoints = doc["breakpoints"].GetArray();
            auto event = std::make_unique<DebuggerProtocol::BreakpointsSet>();

            m_Debugger->RemoveAllBreakpoints();

            for (auto i = 0u; i < breakpoints.Size(); i++)
            {
                auto& bp = breakpoints[i];
                auto line = bp["line"].GetInt();
                auto id = bp["id"].GetInt();
                
                if (m_Debugger->SetBreakpointBySourceLine(id, line))
                {
                    event->AddBreakpoint(line, true);
                }
                else
                {
                    event->AddBreakpoint(line, false);
                }
            }

            SendEvent(event.get());
        }
        else if (typeString == "get-stack")
        {
            auto instruction = m_Debugger->GetCurrentInstruction();

            std::vector<DebuggerProtocol::StackFrame> stackFrames;
            
            if (instruction != nullptr)
            {
                auto& frames = instruction->GetDebugStackFrames();

                for (auto& frame : frames)
                {
                    auto charIndex = 0u;
                    if (frame->m_ASTNode != nullptr)
                    {
                        charIndex = frame->m_ASTNode->GetCharIndex();
                    }

                    auto lineNumber = m_Debugger->CharIndexToLineNumber(charIndex);

                    DebuggerProtocol::StackFrame outFrame;
                    outFrame.m_Name = frame->m_FunctionName;
                    outFrame.m_Id = (unsigned int)frame.get();
                    outFrame.m_LineNumber = lineNumber;

                    for (auto& variable : frame->m_Variables)
                    {
                        auto regId = variable.first;
                        auto& name = variable.second;

                        auto value = m_Debugger->GetRegisterValue(regId);

                        outFrame.m_Variables.push_back(std::make_pair(name, SafePrintf("%", value)));
                    }

                    stackFrames.push_back(outFrame);
                }
            }

            auto event = std::make_unique<DebuggerProtocol::StackTrace>(stackFrames);
            SendEvent(event.get());
        }
        else if (typeString == "next")
        {
            m_Debugger->ContinueToNextAddress();
        }
        else if (typeString == "continue")
        {
            m_Debugger->ResumeExecution();
        }
        else if (typeString == "pause")
        {
            m_Debugger->SuspendExecution();
        }
        else
        {
            LOG_F("Error: Malformed command \"%\", invalid \"type\" property", json);
            return;
        }
    }

    void DebuggerVSCode::SendEvent(DebuggerProtocol::IDebuggerEvent* event)
    {
        StringBuffer buf;
        Writer<StringBuffer> writer(buf);
        event->Serialize(writer);
        SendResponse(buf.GetString());
    }

    void DebuggerVSCode::RunEventsThread()
    {
        auto lastState = m_Debugger->GetState();

        while (true)
        {
            auto currentState = m_Debugger->GetState();
            if (lastState != currentState && currentState == DebuggerThreadState::WaitingAtBreakpoint)
            {
                auto event = std::make_unique<DebuggerProtocol::BreakpointHit>();

                StringBuffer buf;
                Writer<StringBuffer> writer(buf);
                event->Serialize(writer);

                SendResponse(buf.GetString());
            }

            lastState = currentState;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));

            if (m_ShutdownEventsThread)
            {
                m_ShutdownEventsThread = false;
                return;
            }
        }
    }
}
