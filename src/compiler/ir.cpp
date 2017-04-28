#include <cctype>
#include <algorithm>

#include "../log.h"
#include "../ast/ast.h"
#include "ir.h"
#include "ir_optimizer.h"

namespace Langums
{

    bool IRCompiler::Compile(const std::shared_ptr<IASTNode>& ast)
    {
        m_Instructions.clear();
        m_FunctionDeclarations.clear();
        m_GlobalAliases = RegisterAliases();

        if (ast->GetType() != ASTNodeType::Unit)
        {
            throw IRCompilerException("Invalid AST node type, expected Unit");
        }

        auto& unitNodes = ast->GetChildren();

        for (auto& node : unitNodes)
        {
            if (node->GetType() == ASTNodeType::FunctionDeclaration)
            {
                auto fn = (ASTFunctionDeclaration*)node.get();
                auto& name = fn->GetName();

                if (m_FunctionDeclarations.find(name) != m_FunctionDeclarations.end())
                {
                    throw IRCompilerException(SafePrintf("Duplicate function declaration \"%\"", name));
                }

                m_FunctionDeclarations.insert(std::make_pair(name, fn));
            }
        }

        if (m_FunctionDeclarations.find("main") == m_FunctionDeclarations.end())
        {
            throw IRCompilerException("main() function not found");
        }

        auto i = 0u;
        for (auto& pair : m_FunctionDeclarations)
        {
            m_FunctionIndices.insert(std::make_pair(pair.second, i));
            i++;
        }

        auto nextSwitchId = (int)Switch_ReservedEnd;

        for (auto& node : unitNodes)
        {
            if (node->GetType() == ASTNodeType::EventDeclaration)
            {
                auto eventDeclaration = (ASTEventDeclaration*)node.get();
                auto conditionsCount = eventDeclaration->GetConditionsCount();

                if (conditionsCount > MAX_EVENT_CONDITIONS)
                {
                    throw IRCompilerException("Maximum event conditions reached (63 conditions per event)");
                }

                EmitInstruction(new IREventInstruction(nextSwitchId++, conditionsCount), m_Instructions);

                for (auto i = 0u; i < conditionsCount; i++)
                {
                    auto condition = (ASTEventCondition*)eventDeclaration->GetCondition(i).get();
                    auto& name = condition->GetName();

                    if (name == "bring")
                    {
                        auto arg0 = condition->GetArgument(0);
                        if (arg0->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException("Something other than an identifier passed as first argument to bring(), expected player name");
                        }

                        auto playerName = (ASTIdentifier*)arg0.get();
                        auto playerId = PlayerNameToId(playerName->GetName());

                        auto arg1 = condition->GetArgument(1);
                        if (arg1->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException("Something other than an identifier passed as second argument to bring(), expected comparison type");
                        }

                        auto comparisonType = (ASTIdentifier*)arg1.get();
                        auto comparisonName = comparisonType->GetName();

                        ConditionComparison comparison;
                        if (comparisonName == "AtLeast")
                        {
                            comparison = ConditionComparison::AtLeast;
                        }
                        else if (comparisonName == "AtMost")
                        {
                            comparison = ConditionComparison::AtMost;
                        }
                        else if (comparisonName == "Exactly")
                        {
                            comparison = ConditionComparison::Exactly;
                        }
                        else
                        {
                            throw IRCompilerException("Invalid comparison type, expected one of AtLeast, AtMost or Exactly");
                        }

                        auto arg2 = condition->GetArgument(2);
                        if (arg2->GetType() != ASTNodeType::NumberLiteral)
                        {
                            throw IRCompilerException("Something other than a number literal passed as third argument to bring(), expected quantity");
                        }

                        auto quantity = (ASTNumberLiteral*)arg2.get();

                        auto arg3 = condition->GetArgument(3);
                        if (arg3->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException("Something other than an identifier passed as fourth argument to bring(), expected unit type");
                        }

                        auto unitName = (ASTIdentifier*)arg3.get();
                        auto unitId = UnitNameToId(unitName->GetName());
                        if (unitId == -1)
                        {
                            throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to bring()", unitName->GetName()));
                        }

                        auto arg4 = condition->GetArgument(4);
                        if (arg4->GetType() != ASTNodeType::StringLiteral)
                        {
                            throw IRCompilerException("Something other than a string literal passed as fifth argument to bring(), expected location name");
                        }

                        auto location = (ASTStringLiteral*)arg4.get();
                        auto& locationName = location->GetValue();

                        EmitInstruction(new IRBringCondInstruction(playerId, unitId, locationName, comparison, quantity->GetValue()), m_Instructions);
                    }
                    else if (name == "commands" || name == "killed" || name == "deaths")
                    {
                        auto arg0 = condition->GetArgument(0);
                        if (arg0->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", name));
                        }

                        auto playerName = (ASTIdentifier*)arg0.get();
                        auto playerId = PlayerNameToId(playerName->GetName());

                        auto arg1 = condition->GetArgument(1);
                        if (arg1->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected comparison type", name));
                        }

                        auto comparisonType = (ASTIdentifier*)arg1.get();
                        auto comparisonName = comparisonType->GetName();

                        ConditionComparison comparison;
                        if (comparisonName == "AtLeast")
                        {
                            comparison = ConditionComparison::AtLeast;
                        }
                        else if (comparisonName == "AtMost")
                        {
                            comparison = ConditionComparison::AtMost;
                        }
                        else if (comparisonName == "Exactly")
                        {
                            comparison = ConditionComparison::Exactly;
                        }
                        else
                        {
                            throw IRCompilerException("Invalid comparison type, expected one of AtLeast, AtMost or Exactly");
                        }

                        auto arg2 = condition->GetArgument(2);
                        if (arg2->GetType() != ASTNodeType::NumberLiteral)
                        {
                            throw IRCompilerException(SafePrintf("Something other than a number literal passed as third argument to %(), expected quantity", name));
                        }

                        auto quantity = (ASTNumberLiteral*)arg2.get();

                        auto arg3 = condition->GetArgument(3);
                        if (arg3->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException(SafePrintf("Something other than an identifier passed as fourth argument to %(), expected unit type", name));
                        }

                        auto unitName = (ASTIdentifier*)arg3.get();
                        auto unitId = UnitNameToId(unitName->GetName());
                        if (unitId == -1)
                        {
                            throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), name));
                        }

                        if (name == "commands")
                        {
                            EmitInstruction(new IRCmdCondInstruction(playerId, unitId, comparison, quantity->GetValue()), m_Instructions);
                        }
                        else if (name == "killed")
                        {
                            EmitInstruction(new IRKillCondInstruction(playerId, unitId, comparison, quantity->GetValue()), m_Instructions);
                        }
                        else if (name == "deaths")
                        {
                            EmitInstruction(new IRDeathCondInstruction(playerId, unitId, comparison, quantity->GetValue()), m_Instructions);
                        }
                    }
                    else if (name == "commands_least" || name == "commands_most")
                    {
                        auto arg0 = condition->GetArgument(0);
                        if (arg0->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", name));
                        }

                        auto playerName = (ASTIdentifier*)arg0.get();
                        auto playerId = PlayerNameToId(playerName->GetName());

                        auto arg1 = condition->GetArgument(1);
                        if (arg1->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected unit type", name));
                        }

                        auto unitName = (ASTIdentifier*)arg1.get();
                        auto unitId = UnitNameToId(unitName->GetName());
                        if (unitId == -1)
                        {
                            throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), name));
                        }

                        std::string locationName;

                        if (condition->GetChildCount() > 2)
                        {
                            auto arg2 = condition->GetArgument(2);
                            if (arg2->GetType() != ASTNodeType::StringLiteral)
                            {
                                throw IRCompilerException(SafePrintf("Something other than a string literal passed as third argument to %(), expected location name", name));
                            }

                            auto location = (ASTStringLiteral*)arg2.get();
                            locationName = location->GetValue();
                        }

                        if (name == "commands_least")
                        {
                            EmitInstruction(new IRCmdLeastCondInstruction(playerId, unitId, locationName), m_Instructions);
                        }
                        else if (name == "commands_most")
                        {
                            EmitInstruction(new IRCmdMostCondInstruction(playerId, unitId, locationName), m_Instructions);
                        }
                    }
                    else if (name == "killed_least" || name == "killed_most")
                    {
                        auto arg0 = condition->GetArgument(0);
                        if (arg0->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", name));
                        }

                        auto playerName = (ASTIdentifier*)arg0.get();
                        auto playerId = PlayerNameToId(playerName->GetName());

                        auto arg1 = condition->GetArgument(1);
                        if (arg1->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected unit type", name));
                        }

                        auto unitName = (ASTIdentifier*)arg1.get();
                        auto unitId = UnitNameToId(unitName->GetName());
                        if (unitId == -1)
                        {
                            throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), name));
                        }

                        if (name == "killed_least")
                        {
                            EmitInstruction(new IRKillLeastCondInstruction(playerId, unitId), m_Instructions);
                        }
                        else if (name == "killed_most")
                        {
                            EmitInstruction(new IRKillMostCondInstruction(playerId, unitId), m_Instructions);
                        }
                    }
                    else if (name == "accumulate")
                    {
                        auto arg0 = condition->GetArgument(0);
                        if (arg0->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException("Something other than an identifier passed as first argument to accumulate(), expected player name");
                        }

                        auto playerName = (ASTIdentifier*)arg0.get();
                        auto playerId = PlayerNameToId(playerName->GetName());

                        auto arg1 = condition->GetArgument(1);
                        if (arg1->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException("Something other than an identifier passed as second argument to accumulate(), expected comparison type");
                        }

                        auto comparisonType = (ASTIdentifier*)arg1.get();
                        auto comparisonName = comparisonType->GetName();

                        ConditionComparison comparison;
                        if (comparisonName == "AtLeast")
                        {
                            comparison = ConditionComparison::AtLeast;
                        }
                        else if (comparisonName == "AtMost")
                        {
                            comparison = ConditionComparison::AtMost;
                        }
                        else if (comparisonName == "Exactly")
                        {
                            comparison = ConditionComparison::Exactly;
                        }
                        else
                        {
                            throw IRCompilerException("Invalid comparison type, expected one of AtLeast, AtMost or Exactly");
                        }

                        auto arg2 = condition->GetArgument(2);
                        if (arg2->GetType() != ASTNodeType::NumberLiteral)
                        {
                            throw IRCompilerException("Something other than a number literal passed as third argument to accumulate(), expected quantity");
                        }

                        auto quantity = (ASTNumberLiteral*)arg2.get();

                        auto arg3 = condition->GetArgument(3);
                        if (arg3->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException("Something other than an identifier passed as fourth argument to accumulate(), expected resource type");
                        }

                        auto resourceType = ((ASTIdentifier*)arg3.get())->GetName();

                        CHK::ResourceType resType;
                        if (resourceType == "Minerals")
                        {
                            resType = CHK::ResourceType::Ore;
                        }
                        else if (resourceType == "Gas")
                        {
                            resType = CHK::ResourceType::Gas;
                        }
                        else
                        {
                            throw IRCompilerException(SafePrintf("Invalid second argument \"%\" passed to accumulate(), expected Minerals or Gas", resourceType));
                        }

                        EmitInstruction(new IRAccumCondInstruction(playerId, resType, comparison, quantity->GetValue()), m_Instructions);
                    }
                    else if (name == "elapsed_time")
                    {
                        auto arg0 = condition->GetArgument(0);
                        if (arg0->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException("Something other than an identifier passed as first argument to elapsed_time(), expected comparison type");
                        }

                        auto comparisonType = (ASTIdentifier*)arg0.get();
                        auto comparisonName = comparisonType->GetName();

                        ConditionComparison comparison;
                        if (comparisonName == "AtLeast")
                        {
                            comparison = ConditionComparison::AtLeast;
                        }
                        else if (comparisonName == "AtMost")
                        {
                            comparison = ConditionComparison::AtMost;
                        }
                        else if (comparisonName == "Exactly")
                        {
                            comparison = ConditionComparison::Exactly;
                        }
                        else
                        {
                            throw IRCompilerException("Invalid comparison type, expected one of AtLeast, AtMost or Exactly");
                        }

                        auto arg1 = condition->GetArgument(1);
                        if (arg1->GetType() != ASTNodeType::NumberLiteral)
                        {
                            throw IRCompilerException("Something other than a number literal passed as second argument to accumulate(), expected elapsed time quantity");
                        }

                        auto quantity = (ASTNumberLiteral*)arg1.get();

                        EmitInstruction(new IRTimeCondInstruction(comparison, quantity->GetValue()), m_Instructions);
                    }
                    else if (name == "countdown")
                    {
                        auto arg0 = condition->GetArgument(0);
                        if (arg0->GetType() != ASTNodeType::Identifier)
                        {
                            throw IRCompilerException("Something other than an identifier passed as first argument to countdown(), expected comparison type");
                        }

                        auto comparisonType = (ASTIdentifier*)arg0.get();
                        auto comparisonName = comparisonType->GetName();

                        ConditionComparison comparison;
                        if (comparisonName == "AtLeast")
                        {
                            comparison = ConditionComparison::AtLeast;
                        }
                        else if (comparisonName == "AtMost")
                        {
                            comparison = ConditionComparison::AtMost;
                        }
                        else if (comparisonName == "Exactly")
                        {
                            comparison = ConditionComparison::Exactly;
                        }
                        else
                        {
                            throw IRCompilerException("Invalid comparison type, expected one of AtLeast, AtMost or Exactly");
                        }

                        auto arg1 = condition->GetArgument(1);
                        if (arg1->GetType() != ASTNodeType::NumberLiteral)
                        {
                            throw IRCompilerException("Something other than a number literal passed as second argument to countdown(), expected time quantity");
                        }

                        auto quantity = (ASTNumberLiteral*)arg1.get();

                        EmitInstruction(new IRCountdownCondInstruction(comparison, quantity->GetValue()), m_Instructions);
                    }
                    else
                    {
                        throw IRCompilerException(SafePrintf("Unknown condition type \"%\"", name));
                    }
                }
            }
        }

        for (auto& node : unitNodes)
        {
            if (node->GetType() == ASTNodeType::VariableDeclaration)
            {
                auto variable = (ASTVariableDeclaration*)node.get();
                auto& name = variable->GetName();

                if (m_GlobalAliases.GetAlias(name) != -1)
                {
                    throw IRCompilerException(SafePrintf("Duplicate global variable declaration \"%\"", name));
                }

                auto regId = m_GlobalAliases.Allocate(name);

                auto& expression = variable->GetExpression();
                if (expression->GetType() != ASTNodeType::NumberLiteral)
                {
                    throw IRCompilerException(SafePrintf("Trying to initialize global \"%\" with something other than a number literal", name));
                }

                auto number = (ASTNumberLiteral*)expression.get();
                EmitInstruction(new IRSetRegInstruction(regId, number->GetValue()), m_Instructions);
            }
        }

        nextSwitchId = (int)Switch_ReservedEnd;
        m_HasEvents = false;

        m_PollEventsInstructions.clear();

        for (auto& node : unitNodes)
        {
            if (node->GetType() == ASTNodeType::EventDeclaration)
            {
                m_HasEvents = true;
                auto eventDeclaration = (ASTEventDeclaration*)node.get();
                auto body = eventDeclaration->GetBody();
                if (body->GetType() != ASTNodeType::BlockStatement)
                {
                    throw IRCompilerException("Event body must be a block statement");
                }

                std::vector<std::unique_ptr<IIRInstruction>> bodyInstructions;
                EmitBlockStatement((ASTBlockStatement*)body.get(), bodyInstructions, m_GlobalAliases);

                auto switchId = nextSwitchId++;
                EmitInstruction(new IRJmpIfSwNotSetInstruction(switchId, bodyInstructions.size() + 2), m_PollEventsInstructions);

                for (auto& instruction : bodyInstructions)
                {
                    m_PollEventsInstructions.push_back(std::move(instruction));
                }

                EmitInstruction(new IRSetSwInstruction(switchId, 0), m_PollEventsInstructions);
            }
        }

        EmitInstruction(new IRSetSwInstruction(Switch_EventsMutex, false), m_PollEventsInstructions);

        auto main = m_FunctionDeclarations["main"];
        EmitFunction(main, m_Instructions, m_GlobalAliases);
        return true;
    }

    void IRCompiler::Optimize()
    {
        IROptimizer optimizer;
        m_Instructions = optimizer.Process(std::move(m_Instructions));
    }

    void IRCompiler::EmitFunctionCall(ASTFunctionCall* fnCall, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool ignoreReturnValue)
    {
        auto& fnName = fnCall->GetFunctionName();

        if (fnName == "poll_events")
        {
            if (m_HasEvents)
            {
                if (m_PollEventsInstructions.size() == 0)
                {
                    throw IRCompilerException("poll_events() can only be called from a single place");
                }

                EmitInstruction(new IRSetSwInstruction(Switch_EventsMutex, true), instructions);

                for (auto& instruction : m_PollEventsInstructions)
                {
                    instructions.push_back(std::move(instruction));
                }
             
                m_PollEventsInstructions.clear();
            }
        }
        else if (fnName == "rnd256")
        {
            if (!ignoreReturnValue)
            {
                EmitInstruction(new IRRnd256Instruction(), instructions);
            }
        }
        else if (fnName == "end")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("end() called without arguments");
            }

            if (fnCall->GetChildCount() != 2)
            {
                throw IRCompilerException("end() takes exactly two arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg0.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected Victory, Defeat or Draw", fnName));
            }

            auto& endCondition = ((ASTIdentifier*)arg1.get())->GetName();
            EndGameType type = EndGameType::Victory;
            if (endCondition == "Victory")
            {
                type = EndGameType::Victory;
            }
            else if (endCondition == "Defeat")
            {
                type = EndGameType::Defeat;
            }
            else if (endCondition == "Draw")
            {
                type = EndGameType::Draw;
            }
            else
            {
                throw IRCompilerException(SafePrintf("Invalid second argument \"%\" passed to %(), expected Victory, Defeat or Draw", endCondition, fnName));
            }

            EmitInstruction(new IREndGameInstruction(playerId + 1, type), instructions);
        }
        else if (fnName == "set_resource")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_resource() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("set_resource() takes exactly three arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg0.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected Minerals or Gas", fnName));
            }

            auto resource = (ASTIdentifier*)arg1.get();
            auto resourceType = resource->GetName();

            CHK::ResourceType resType;
            if (resourceType == "Minerals")
            {
                resType = CHK::ResourceType::Ore;
            }
            else if (resourceType == "Gas")
            {
                resType = CHK::ResourceType::Gas;
            }
            else
            {
                throw IRCompilerException(SafePrintf("Invalid second argument \"%\" passed to %(), expected Minerals or Gas", resourceType, fnName));
            }

            auto regId = 0;
            bool isValueLiteral = false;

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() == ASTNodeType::NumberLiteral)
            {
                auto number = (ASTNumberLiteral*)arg2.get();
                regId = number->GetValue();
                isValueLiteral = true;
            }
            else
            {
                EmitExpression(arg2.get(), instructions, aliases);
                regId = Reg_StackTop;
            }

            EmitInstruction(new IRSetResourceInstruction(playerId, resType, regId, isValueLiteral), instructions);
        }
        else if (fnName == "add_resource")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("add_resource() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("add_resource() takes exactly three arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg0.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected Minerals or Gas", fnName));
            }

            auto resource = (ASTIdentifier*)arg1.get();
            auto resourceType = resource->GetName();

            CHK::ResourceType resType;
            if (resourceType == "Minerals")
            {
                resType = CHK::ResourceType::Ore;
            }
            else if (resourceType == "Gas")
            {
                resType = CHK::ResourceType::Gas;
            }
            else
            {
                throw IRCompilerException(SafePrintf("Invalid second argument \"%\" passed to %(), expected Minerals or Gas", resourceType, fnName));
            }

            auto regId = 0;
            bool isValueLiteral = false;

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() == ASTNodeType::NumberLiteral)
            {
                auto number = (ASTNumberLiteral*)arg2.get();
                regId = number->GetValue();
                isValueLiteral = true;
            }
            else
            {
                EmitExpression(arg2.get(), instructions, aliases);
                regId = Reg_StackTop;
            }

            EmitInstruction(new IRIncResourceInstruction(playerId, resType, regId, isValueLiteral), instructions);
        }
        else if (fnName == "take_resource")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("take_resource() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("take_resource() takes exactly three arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg0.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected Minerals or Gas", fnName));
            }

            auto resource = (ASTIdentifier*)arg1.get();
            auto resourceType = resource->GetName();

            CHK::ResourceType resType;
            if (resourceType == "Minerals")
            {
                resType = CHK::ResourceType::Ore;
            }
            else if (resourceType == "Gas")
            {
                resType = CHK::ResourceType::Gas;
            }
            else
            {
                throw IRCompilerException(SafePrintf("Invalid second argument \"%\" passed to %(), expected Minerals or Gas", resourceType, fnName));
            }

            auto regId = 0;
            bool isValueLiteral = false;

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() == ASTNodeType::NumberLiteral)
            {
                auto number = (ASTNumberLiteral*)arg2.get();
                regId = number->GetValue();
                isValueLiteral = true;
            }
            else
            {
                EmitExpression(arg2.get(), instructions, aliases);
                regId = Reg_StackTop;
            }

            EmitInstruction(new IRDecResourceInstruction(playerId, resType, regId, isValueLiteral), instructions);
        }
        else if (fnName == "set_countdown")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_countdown() called without arguments");
            }

            if (fnCall->GetChildCount() != 1)
            {
                throw IRCompilerException("set_countdown() takes exactly one argument");
            }

            auto regId = 0;
            bool isValueLiteral = false;

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() == ASTNodeType::NumberLiteral)
            {
                auto number = (ASTNumberLiteral*)arg0.get();
                regId = number->GetValue();
                isValueLiteral = true;
            }
            else
            {
                EmitExpression(arg0.get(), instructions, aliases);
                regId = Reg_StackTop;
            }

            EmitInstruction(new IRSetCountdownInstruction(regId, isValueLiteral), instructions);
        }
        else if (fnName == "set_deaths")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_deaths() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("set_deaths() takes exactly three arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg0.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected unit name", fnName));
            }

            auto unitName = (ASTIdentifier*)arg1.get();
            auto unitId = UnitNameToId(unitName->GetName());
            if (unitId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), fnName));
            }

            auto regId = 0;
            bool isValueLiteral = false;

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() == ASTNodeType::NumberLiteral)
            {
                auto number = (ASTNumberLiteral*)arg2.get();
                regId = number->GetValue();
                isValueLiteral = true;
            }
            else
            {
                EmitExpression(arg2.get(), instructions, aliases);
                regId = Reg_StackTop;
            }

            EmitInstruction(new IRSetDeathsInstruction(playerId, unitId, regId, isValueLiteral), instructions);
        }
        else if (fnName == "add_deaths")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("inc_deaths() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("inc_deaths() takes exactly three arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg0.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected unit name", fnName));
            }

            auto unitName = (ASTIdentifier*)arg1.get();
            auto unitId = UnitNameToId(unitName->GetName());
            if (unitId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), fnName));
            }

            auto regId = 0;
            bool isValueLiteral = false;

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() == ASTNodeType::NumberLiteral)
            {
                auto number = (ASTNumberLiteral*)arg2.get();
                regId = number->GetValue();
                isValueLiteral = true;
            }
            else
            {
                EmitExpression(arg2.get(), instructions, aliases);
                regId = Reg_StackTop;
            }

            EmitInstruction(new IRIncDeathsInstruction(playerId, unitId, regId, isValueLiteral), instructions);
        }
        else if (fnName == "remove_deaths")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("remove_deaths() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("remove_deaths() takes exactly three arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg0.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected unit name", fnName));
            }

            auto unitName = (ASTIdentifier*)arg1.get();
            auto unitId = UnitNameToId(unitName->GetName());
            if (unitId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), fnName));
            }

            auto regId = 0;
            bool isValueLiteral = false;

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() == ASTNodeType::NumberLiteral)
            {
                auto number = (ASTNumberLiteral*)arg2.get();
                regId = number->GetValue();
                isValueLiteral = true;
            }
            else
            {
                EmitExpression(arg2.get(), instructions, aliases);
                regId = Reg_StackTop;
            }

            EmitInstruction(new IRDecDeathsInstruction(playerId, unitId, regId, isValueLiteral), instructions);
        }
        else if (fnName == "talking_portrait")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("talking_portrait() called without arguments");
            }

            if (fnCall->GetChildCount() != 2)
            {
                throw IRCompilerException("talking_portrait() takes exactly two arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected unit name", fnName));
            }

            auto unitName = (ASTIdentifier*)arg0.get();
            auto unitId = UnitNameToId(unitName->GetName());
            if (unitId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::NumberLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a number literal passed as second argument to %(), expected amount of time", fnName));
            }

            auto time = (ASTNumberLiteral*)arg1.get();
            EmitInstruction(new IRTalkInstruction(unitId, time->GetValue() * 1000), instructions);
        }
        else if (fnName == "center_view")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("center_view() called without arguments");
            }

            if (fnCall->GetChildCount() != 2)
            {
                throw IRCompilerException("center_view() takes exactly two arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg0.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as fourth argument to %(), expected location name", fnName));
            }

            auto locationName = (ASTStringLiteral*)arg1.get();
            auto locName = locationName->GetValue();

            EmitInstruction(new IRCenterViewInstruction(playerId, locName), instructions);
        }
        else if (fnName == "ping")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("ping() called without arguments");
            }

            if (fnCall->GetChildCount() != 2)
            {
                throw IRCompilerException("ping() takes exactly two arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg0.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as fourth argument to %(), expected location name", fnName));
            }

            auto locationName = (ASTStringLiteral*)arg1.get();
            auto locName = locationName->GetValue();

            EmitInstruction(new IRPingInstruction(playerId, locName), instructions);
        }
        else if (fnName == "print")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("print() called without arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal passed as first argument to %()", fnName));
            }

            auto msg = (ASTStringLiteral*)arg0.get();
            auto playerId = 1;

            if (fnCall->GetChildCount() >= 2)
            {
                auto arg1 = fnCall->GetArgument(1);
                if (arg1->GetType() != ASTNodeType::Identifier)
                {
                    throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected player name", fnName));
                }

                auto playerName = (ASTIdentifier*)arg1.get();
                playerId = PlayerNameToId(playerName->GetName()) + 1;
                if (playerId == -1)
                {
                    throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
                }
            }

            EmitInstruction(new IRDisplayMsgInstruction(msg->GetValue(), playerId), instructions);
        }
        else if (fnName == "sleep")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("sleep() called without arguments");
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::NumberLiteral)
            {
                throw IRCompilerException("Something other than a number literal passed to sleep()");
            }

            auto milliseconds = (ASTNumberLiteral*)arg0.get();
            EmitInstruction(new IRWaitInstruction(milliseconds->GetValue()), instructions);
        }
        else if (fnName == "spawn" || fnName == "kill" || fnName == "remove")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName));
            }

            if (fnName == "spawn" && fnCall->GetChildCount() != 4)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 4 arguments", fnName));
            }
            else if (fnName == "kill" && (fnCall->GetChildCount() < 3 || fnCall->GetChildCount() > 4))
            {
                throw IRCompilerException(SafePrintf("%() takes 3 or 4 arguments", fnName));
            }
            else if (fnName == "remove" && (fnCall->GetChildCount() < 3 || fnCall->GetChildCount() > 4))
            {
                throw IRCompilerException(SafePrintf("%() takes 3 or 4 arguments", fnName));
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected unit name", fnName));
            }

            auto unitName = (ASTIdentifier*)arg0.get();
            auto unitId = UnitNameToId(unitName->GetName());
            if (unitId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg1.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto regId = 0;
            bool isLiteral = false;

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() == ASTNodeType::NumberLiteral)
            {
                auto unitQuantity = (ASTNumberLiteral*)arg2.get();
                regId = unitQuantity->GetValue();

                if (regId <= 0)
                {
                    throw IRCompilerException(SafePrintf("Trying to %() zero or less units", fnName));
                }

                isLiteral = true;
            }
            else
            {
                EmitExpression(arg2.get(), instructions, aliases);
                isLiteral = false;
                regId = Reg_StackTop;
            }
            
            std::string locName;

            if (fnCall->GetChildCount() == 4)
            {
                auto arg3 = fnCall->GetArgument(3);
                if (arg3->GetType() != ASTNodeType::StringLiteral)
                {
                    throw IRCompilerException(SafePrintf("Something other than a string literal as fourth argument to %(), expected location name", fnName));
                }

                auto locationName = (ASTStringLiteral*)arg3.get();
                locName = locationName->GetValue();
            }

            if (fnName == "spawn")
            {
                EmitInstruction(new IRSpawnInstruction(playerId, unitId, regId, isLiteral, locName), instructions);
            }
            else if (fnName == "kill")
            {
                EmitInstruction(new IRKillInstruction(playerId, unitId, regId, isLiteral, locName), instructions);
            }
            else if (fnName == "remove")
            {
                EmitInstruction(new IRRemoveInstruction(playerId, unitId, regId, isLiteral, locName), instructions);
            }
        }
        else if (fnName == "move")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName));
            }

            if (fnCall->GetChildCount() != 5)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 5 arguments", fnName));
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected unit name", fnName));
            }

            auto unitName = (ASTIdentifier*)arg0.get();
            auto unitId = UnitNameToId(unitName->GetName());
            if (unitId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg1.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto regId = 0;
            bool isLiteral = false;

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() == ASTNodeType::NumberLiteral)
            {
                auto unitQuantity = (ASTNumberLiteral*)arg2.get();
                regId = unitQuantity->GetValue();

                if (regId <= 0)
                {
                    throw IRCompilerException(SafePrintf("Trying to %() zero or less units", fnName));
                }

                isLiteral = true;
            }
            else
            {
                EmitExpression(arg2.get(), instructions, aliases);
                isLiteral = false;
                regId = Reg_StackTop;
            }

            auto arg3 = fnCall->GetArgument(3);
            if (arg3->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as fourth argument to %(), expected location name", fnName));
            }

            auto srcLocationName = (ASTStringLiteral*)arg3.get();
            auto srcLocName = srcLocationName->GetValue();

            auto arg4 = fnCall->GetArgument(4);
            if (arg4->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as fifth argument to %(), expected location name", fnName));
            }

            auto dstLocationName = (ASTStringLiteral*)arg4.get();
            auto dstLocName = dstLocationName->GetValue();

            EmitInstruction(new IRMoveInstruction(playerId, unitId, regId, isLiteral, srcLocName, dstLocName), instructions);
        }
        else if (fnName == "order")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName));
            }

            if (fnCall->GetChildCount() != 5)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 5 arguments", fnName));
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected unit name", fnName));
            }

            auto unitName = (ASTIdentifier*)arg0.get();
            auto unitId = UnitNameToId(unitName->GetName());
            if (unitId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg1.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as third argument to %(), expected order type", fnName));
            }

            auto order = (ASTIdentifier*)arg2.get();
            auto orderType = order->GetName();
            CHK::TriggerActionState orderValue;
            if (orderType == "Move")
            {
                orderValue = CHK::TriggerActionState::Move;
            }
            else if (orderType == "Attack")
            {
                orderValue = CHK::TriggerActionState::Attack;
            }
            else if (orderType == "Patrol")
            {
                orderValue = CHK::TriggerActionState::Patrol;
            }
            else
            {
                throw IRCompilerException(SafePrintf("Invalid order passed as third argument to %(), expected Move, Attack or Patrol", fnName));
            }

            auto arg3 = fnCall->GetArgument(3);
            if (arg3->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as fourth argument to %(), expected location name", fnName));
            }

            auto srcLocationName = (ASTStringLiteral*)arg3.get();
            auto srcLocName = srcLocationName->GetValue();

            auto arg4 = fnCall->GetArgument(4);
            if (arg4->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as fifth argument to %(), expected location name", fnName));
            }

            auto dstLocationName = (ASTStringLiteral*)arg4.get();
            auto dstLocName = dstLocationName->GetValue();

            EmitInstruction(new IROrderInstruction(playerId, unitId, orderValue, srcLocName, dstLocName), instructions);
        }
        else if (fnName == "modify")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName));
            }

            if (fnCall->GetChildCount() != 6)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 6 arguments", fnName));
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected unit name", fnName));
            }

            auto unitName = (ASTIdentifier*)arg0.get();
            auto unitId = UnitNameToId(unitName->GetName());
            if (unitId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg1.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto regId = 0;
            bool isLiteral = false;

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() == ASTNodeType::NumberLiteral)
            {
                auto unitQuantity = (ASTNumberLiteral*)arg2.get();
                regId = unitQuantity->GetValue();

                if (regId <= 0)
                {
                    throw IRCompilerException(SafePrintf("Trying to %() zero or less units", fnName));
                }

                isLiteral = true;
            }
            else
            {
                EmitExpression(arg2.get(), instructions, aliases);
                isLiteral = false;
                regId = Reg_StackTop;
            }

            auto arg3 = fnCall->GetArgument(3);
            if (arg3->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as third argument to %(), expected Health, Energy, Shields or Hangar", fnName));
            }

            auto modify = (ASTIdentifier*)arg3.get();
            auto modifyValue = modify->GetName();
            ModifyType modifyType;
            if (modifyValue == "Health")
            {
                modifyType = ModifyType::HitPoints;
            }
            else if (modifyValue == "Energy")
            {
                modifyType = ModifyType::Energy;
            }
            else if (modifyValue == "Shields")
            {
                modifyType = ModifyType::ShieldPoints;
            }
            else if (modifyValue == "Hangar")
            {
                modifyType = ModifyType::HangarCount;
            }
            else
            {
                throw IRCompilerException(SafePrintf("Invalid fourth argument to %(), expected Health, Energy, Shields or Hangar", fnName));
            }

            auto arg4 = fnCall->GetArgument(4);
            if (arg4->GetType() != ASTNodeType::NumberLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a number literal as fourth argument to %(), expected amount", fnName));
            }

            auto amount = (ASTNumberLiteral*)arg4.get();

            auto arg5 = fnCall->GetArgument(5);
            if (arg5->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as fifth argument to %(), expected location name", fnName));
            }

            auto locationName = (ASTStringLiteral*)arg5.get();
            auto locName = locationName->GetValue();

            EmitInstruction(new IRModifyInstruction(playerId, unitId, regId, isLiteral, amount->GetValue(), modifyType, locName), instructions);
        }
        else if (fnName == "give")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName));
            }

            if (fnCall->GetChildCount() != 5)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 5 arguments", fnName));
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected unit name", fnName));
            }

            auto unitName = (ASTIdentifier*)arg0.get();
            auto unitId = UnitNameToId(unitName->GetName());
            if (unitId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected player name", fnName));
            }

            auto srcPlayerName = (ASTIdentifier*)arg1.get();
            auto srcPlayerId = PlayerNameToId(srcPlayerName->GetName());
            if (srcPlayerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid source player name \"%\" passed to %()", srcPlayerName->GetName(), fnName));
            }

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as third argument to %(), expected player name", fnName));
            }

            auto dstPlayerName = (ASTIdentifier*)arg2.get();
            auto dstPlayerId = PlayerNameToId(dstPlayerName->GetName());
            if (dstPlayerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid destination player name \"%\" passed to %()", dstPlayerName->GetName(), fnName));
            }

            auto regId = 0;
            bool isLiteral = false;

            auto arg3 = fnCall->GetArgument(3);
            if (arg3->GetType() == ASTNodeType::NumberLiteral)
            {
                auto unitQuantity = (ASTNumberLiteral*)arg3.get();
                regId = unitQuantity->GetValue();

                if (regId <= 0)
                {
                    throw IRCompilerException(SafePrintf("Trying to %() zero or less units", fnName));
                }

                isLiteral = true;
            }
            else
            {
                EmitExpression(arg3.get(), instructions, aliases);
                isLiteral = false;
                regId = Reg_StackTop;
            }

            auto arg4 = fnCall->GetArgument(4);
            if (arg4->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as fifth argument to %(), expected location name", fnName));
            }

            auto locationName = (ASTStringLiteral*)arg4.get();
            auto locName = locationName->GetValue();

            EmitInstruction(new IRGiveInstruction(srcPlayerId, dstPlayerId, unitId, regId, isLiteral, locName), instructions);
        }
        else if (fnName == "move_loc")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName));
            }

            if (fnCall->GetChildCount() != 4)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 4 arguments", fnName));
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as first argument to %(), expected unit name", fnName));
            }

            auto unitName = (ASTIdentifier*)arg0.get();
            auto unitId = UnitNameToId(unitName->GetName());
            if (unitId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed to %()", unitName->GetName(), fnName));
            }

            auto arg1 = fnCall->GetArgument(1);
            if (arg1->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than an identifier passed as second argument to %(), expected player name", fnName));
            }

            auto playerName = (ASTIdentifier*)arg1.get();
            auto playerId = PlayerNameToId(playerName->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid player name \"%\" passed to %()", playerName->GetName(), fnName));
            }

            auto arg2 = fnCall->GetArgument(2);
            if (arg2->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as third argument to %(), expected location name", fnName));
            }

            auto srcLocationName = (ASTStringLiteral*)arg2.get();
            auto srcLocName = srcLocationName->GetValue();

            auto arg3 = fnCall->GetArgument(3);
            if (arg3->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as fourth argument to %(), expected location name", fnName));
            }

            auto dstLocationName = (ASTStringLiteral*)arg3.get();
            auto dstLocName = dstLocationName->GetValue();

            EmitInstruction(new IRMoveLocInstruction(playerId, unitId, srcLocName, dstLocName), instructions);
        }
        else if (m_FunctionDeclarations.find(fnName) != m_FunctionDeclarations.end())
        {
            auto declaration = m_FunctionDeclarations[fnName];
            auto argCount = declaration->GetArgumentCount();
            auto& argNames = declaration->GetArguments();

            if (fnCall->GetChildCount() != argCount)
            {
                throw IRCompilerException(SafePrintf("Argument count mismatch when calling function \"%\", got % but expected %",
                    fnName, fnCall->GetChildCount(), argCount));
            }

            for (auto i = 0u; i < argCount; i++)
            {
                auto& arg = fnCall->GetArgument(i);
                EmitExpression(arg.get(), instructions, aliases);
            }

            EmitFunction(declaration, instructions, aliases);
            if (ignoreReturnValue)
            {
                EmitInstruction(new IRPopInstruction(), instructions);
            }
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid function name \"%\"", fnName));
        }
    }

    void IRCompiler::EmitBinaryExpression(ASTBinaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases)
    {
        auto& lhs = expression->GetLHSValue();
        auto& rhs = expression->GetRHSValue();

        auto op = expression->GetOperator();
        if (op == OperatorType::Add)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitExpression(rhs.get(), instructions, aliases);
            EmitInstruction(new IRAddInstruction(), instructions);
        }
        else if (op == OperatorType::Subtract)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitExpression(rhs.get(), instructions, aliases);
            EmitInstruction(new IRSubInstruction(), instructions);
        }
        else if (op == OperatorType::Multiply)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitExpression(rhs.get(), instructions, aliases);
            EmitInstruction(new IRPopInstruction(Reg_Temp0), instructions);
            EmitInstruction(new IRPopInstruction(Reg_Temp1), instructions);

            EmitInstruction(new IRDecRegInstruction(Reg_Temp0, 1), instructions);
            EmitInstruction(new IRPushInstruction(Reg_Temp1), instructions);
            EmitInstruction(new IRAddInstruction(), instructions);
            EmitInstruction(new IRJmpIfNotEqZeroInstruction(Reg_Temp0, -3), instructions);
        }
        else if (op == OperatorType::Divide)
        {
            EmitExpression(rhs.get(), instructions, aliases);
            EmitExpression(lhs.get(), instructions, aliases);

            EmitInstruction(new IRSetRegInstruction(Reg_Temp1, 0), instructions);
            EmitInstruction(new IRPopInstruction(Reg_Temp0), instructions);
            EmitInstruction(new IRPushInstruction(Reg_Temp0), instructions);
            EmitInstruction(new IRIncRegInstruction(Reg_Temp1, 1), instructions);
            EmitInstruction(new IRSubInstruction(), instructions);
            EmitInstruction(new IRJmpIfSwNotSetInstruction(Switch_ArithmeticUnderflow, -3), instructions);
            EmitInstruction(new IRPopInstruction(), instructions);
            EmitInstruction(new IRDecRegInstruction(Reg_Temp1, 1), instructions);
            EmitInstruction(new IRPushInstruction(Reg_Temp1), instructions);
        }
        else if (op == OperatorType::Equals)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitExpression(rhs.get(), instructions, aliases);
            EmitInstruction(new IRSubInstruction(), instructions);
            EmitInstruction(new IRJmpIfNotEqZeroInstruction(Reg_StackTop, 4), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions);
            EmitInstruction(new IRJmpIfSwNotSetInstruction(Switch_ArithmeticUnderflow, 2), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions);
        }
        else if (op == OperatorType::NotEquals)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitExpression(rhs.get(), instructions, aliases);
            EmitInstruction(new IRSubInstruction(), instructions);
            EmitInstruction(new IRJmpIfNotEqZeroInstruction(Reg_StackTop, 3), instructions);
            EmitInstruction(new IRJmpIfSwNotSetInstruction(Switch_ArithmeticUnderflow, 2), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions);
        }
        else if (op == OperatorType::LessThanOrEquals)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitExpression(rhs.get(), instructions, aliases);
            EmitInstruction(new IRSubInstruction(), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions);
            EmitInstruction(new IRJmpIfSwNotSetInstruction(Switch_ArithmeticUnderflow, 2), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions);
        }
        else if (op == OperatorType::LessThan)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitExpression(rhs.get(), instructions, aliases);
            EmitInstruction(new IRDecRegInstruction(Reg_StackTop+1, 1), instructions);
            EmitInstruction(new IRSubInstruction(), instructions);
            EmitInstruction(new IRJmpIfSwNotSetInstruction(Switch_ArithmeticUnderflow, 3), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions);
            EmitInstruction(new IRJmpInstruction(2), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions);
        }
        else if (op == OperatorType::GreaterThanOrEquals)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitExpression(rhs.get(), instructions, aliases);
            EmitInstruction(new IRSubInstruction(), instructions);
            EmitInstruction(new IRJmpIfEqZeroInstruction(Reg_StackTop, 4), instructions);
            EmitInstruction(new IRJmpIfSwSetInstruction(Switch_ArithmeticUnderflow, 3), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions);
            EmitInstruction(new IRJmpInstruction(2), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions);
        }
        else if (op == OperatorType::GreaterThan)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitExpression(rhs.get(), instructions, aliases);
            EmitInstruction(new IRSubInstruction(), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions);
            EmitInstruction(new IRJmpIfSwNotSetInstruction(Switch_ArithmeticUnderflow, 2), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions);
        }
        else
        {
            throw IRCompilerException("Unsupported operator");
        }
    }

    void IRCompiler::EmitUnaryExpression(ASTUnaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases)
    {
        auto& lhs = expression->GetValue();

        auto op = expression->GetOperator();
        if (op == OperatorType::Not)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitInstruction(new IRJmpIfEqZeroInstruction(Reg_StackTop, 3), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions);
            EmitInstruction(new IRJmpInstruction(2), instructions);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions);
        }
        else if (op == OperatorType::PostfixIncrement)
        {
            if (lhs->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException("Postfix operator used on something other than an identifier");
            }

            auto identifier = (ASTIdentifier*)lhs.get();
            auto regId = RegisterNameToIndex(identifier->GetName(), aliases);
            if (regId == -1)
            {
                throw IRCompilerException(SafePrintf("Unknown name \"%\"", identifier->GetName()));
            }

            EmitInstruction(new IRPushInstruction(regId), instructions);
            EmitInstruction(new IRIncRegInstruction(regId, 1), instructions);
        }
        else if (op == OperatorType::PostfixDecrement)
        {
            if (lhs->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException("Postfix operator used on something other than an identifier");
            }

            auto identifier = (ASTIdentifier*)lhs.get();
            auto regId = RegisterNameToIndex(identifier->GetName(), aliases);
            if (regId == -1)
            {
                throw IRCompilerException(SafePrintf("Unknown name \"%\"", identifier->GetName()));
            }

            EmitInstruction(new IRPushInstruction(regId), instructions);
            EmitInstruction(new IRDecRegInstruction(regId, 1), instructions);
        }
        else
        {
            throw IRCompilerException("Unsupported operator");
        }
    }

    void IRCompiler::EmitExpression(IASTNode* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases)
    {
        if (expression->GetType() == ASTNodeType::NumberLiteral)
        {
            auto number = (ASTNumberLiteral*)expression;
            EmitInstruction(new IRPushInstruction(number->GetValue(), true), instructions);
        }
        else if (expression->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)expression;
            auto regId = RegisterNameToIndex(identifier->GetName(), aliases);
            EmitInstruction(new IRPushInstruction(regId), instructions);
        }
        else if (expression->GetType() == ASTNodeType::BinaryExpression)
        {
            EmitBinaryExpression((ASTBinaryExpression*)expression, instructions, aliases);
        }
        else if (expression->GetType() == ASTNodeType::UnaryExpression)
        {
            EmitUnaryExpression((ASTUnaryExpression*)expression, instructions, aliases);
        }
        else if (expression->GetType() == ASTNodeType::FunctionCall)
        {
            EmitFunctionCall((ASTFunctionCall*)expression, instructions, aliases, false);
        }
        else
        {
            throw IRCompilerException("Unsupported expression value");
        }
    }

    unsigned int IRCompiler::EmitBlockStatement(ASTBlockStatement* blockStatement, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases)
    {
        auto startIndex = instructions.size();

        auto& statements = blockStatement->GetChildren();
        
        std::vector<std::string> localVariables;

        // we want to hoist variable declarations to the top of the block
        // so we'll iterate those first
        for (auto& statement : statements)
        {
            if (statement->GetType() == ASTNodeType::VariableDeclaration)
            {
                auto variableDeclaration = (ASTVariableDeclaration*)statement.get();

                auto& name = variableDeclaration->GetName();
                aliases.Allocate(name);
                localVariables.push_back(name);
            }
        }

        int statementIndex = 0;
        for (auto& statement : statements)
        {
            if (statement->GetType() == ASTNodeType::VariableDeclaration)
            {
                auto variableDeclaration = (ASTVariableDeclaration*)statement.get();
                auto& expression = variableDeclaration->GetExpression();

                auto regId = RegisterNameToIndex(variableDeclaration->GetName(), aliases);
                if (expression->GetType() == ASTNodeType::NumberLiteral)
                {
                    auto number = (ASTNumberLiteral*)expression.get();
                    EmitInstruction(new IRSetRegInstruction(regId, number->GetValue()), instructions);
                }
                else
                {
                    EmitExpression(expression.get(), instructions, aliases);
                    EmitInstruction(new IRPopInstruction(regId), instructions);
                }
            }
            else if (statement->GetType() == ASTNodeType::AssignmentExpression)
            {
                auto expression = (ASTAssignmentExpression*)statement.get();

                auto& lhs = expression->GetLHSValue();
                if (!IsRegisterName(lhs.GetName(), aliases))
                {
                    throw IRCompilerException("Assignment expression contains non-register on the left side");
                }

                auto lhsRegIndex = RegisterNameToIndex(lhs.GetName(), aliases);

                auto& rhs = expression->GetRHSValue();

                if (rhs->GetType() == ASTNodeType::NumberLiteral)
                {
                    auto number = (ASTNumberLiteral*)rhs.get();
                    EmitInstruction(new IRSetRegInstruction(lhsRegIndex, number->GetValue()), instructions);
                }
                else if (rhs->GetType() == ASTNodeType::Identifier)
                {
                    auto rhsRegister = (ASTIdentifier*)rhs.get();
                    auto rhsRegIndex = RegisterNameToIndex(rhsRegister->GetName(), aliases);
                    EmitInstruction(new IRCopyRegInstruction(lhsRegIndex, rhsRegIndex), instructions);
                }
                else
                {
                    EmitExpression(rhs.get(), instructions, aliases);
                    EmitInstruction(new IRPopInstruction(lhsRegIndex), instructions);
                }
            }
            else if (statement->GetType() == ASTNodeType::UnaryExpression)
            {
                auto expression = (ASTUnaryExpression*)statement.get();

                auto op = expression->GetOperator();
                if (op != OperatorType::PostfixDecrement && op != OperatorType::PostfixIncrement)
                {
                    throw IRCompilerException("Only postfix increment & decrement operators are allowed in an expression statement");
                }

                auto identifier = (ASTIdentifier*)expression->GetChild(0).get();
                auto regId = RegisterNameToIndex(identifier->GetName(), aliases);
                if (regId == -1)
                {
                    throw IRCompilerException(SafePrintf("Unknown name \"%\"", regId));
                }

                if (op == OperatorType::PostfixIncrement)
                {
                    EmitInstruction(new IRIncRegInstruction(regId, 1), instructions);
                }
                else if (op == OperatorType::PostfixDecrement)
                {
                    EmitInstruction(new IRDecRegInstruction(regId, 1), instructions);
                }
            }
            else if (statement->GetType() == ASTNodeType::FunctionCall)
            {
                EmitFunctionCall((ASTFunctionCall*)statement.get(), instructions, aliases, true);
            }
            else if (statement->GetType() == ASTNodeType::IfStatement)
            {
                auto ifStatement = (ASTIfStatement*)statement.get();
                auto& expression = ifStatement->GetExpression();
                auto& body = ifStatement->GetBody();

                if (body->GetType() != ASTNodeType::BlockStatement)
                {
                    throw IRCompilerException("Invalid AST. If statement body must be a block statement");
                }

                std::vector<std::unique_ptr<IIRInstruction>> bodyInstructions;
                EmitBlockStatement((ASTBlockStatement*)body.get(), bodyInstructions, aliases);

                if (bodyInstructions.size() == 0)
                {
                    throw IRCompilerException("Disallowed if statement with empty body");
                }

                std::vector<std::unique_ptr<IIRInstruction>> elseBodyInstructions;
                auto& elseBody = ifStatement->GetElseBody();
                if (elseBody != nullptr)
                {
                    if (elseBody->GetType() != ASTNodeType::BlockStatement)
                    {
                        throw IRCompilerException("Invalid AST. If-else statement body must be a block statement");
                    }

                    EmitBlockStatement((ASTBlockStatement*)elseBody.get(), elseBodyInstructions, aliases);

                    if (elseBodyInstructions.size() == 0)
                    {
                        throw IRCompilerException("Disallowed if-else statement with empty body");
                    }
                }

                if (expression->GetType() == ASTNodeType::NumberLiteral)
                {
                    auto numberLiteral = (ASTNumberLiteral*)expression.get();
                    if (numberLiteral->GetValue() > 0)
                    {
                        for (auto& instruction : bodyInstructions)
                        {
                            instructions.push_back(std::move(instruction));
                        }
                    }
                    else
                    {
                        for (auto& instruction : elseBodyInstructions)
                        {
                            instructions.push_back(std::move(instruction));
                        }
                    }
                }
                else if (expression->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)expression.get();

                    if (bodyInstructions.size() == 0)
                    {
                        throw IRCompilerException("Disallowed if statement with empty body");
                    }

                    auto regId = RegisterNameToIndex(identifier->GetName(), aliases);
                    auto offset = 1;
                    if (elseBodyInstructions.size() > 0)
                    {
                        offset = 2;
                    }

                    EmitInstruction(new IRJmpIfEqZeroInstruction(regId, bodyInstructions.size() + 2), instructions);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    if (elseBodyInstructions.size() > 0)
                    {
                        EmitInstruction(new IRJmpInstruction(elseBodyInstructions.size() + 1), instructions);
                    }

                    for (auto& instruction : elseBodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }
                }
                else if (expression->GetType() == ASTNodeType::BinaryExpression)
                {
                    auto binaryExpression = (ASTBinaryExpression*)expression.get();
                    EmitBinaryExpression(binaryExpression, instructions, aliases);
                    EmitInstruction(new IRPopInstruction(Reg_Temp0), instructions);

                    auto offset = 1;
                    if (elseBodyInstructions.size() > 0)
                    {
                        offset = 2;
                    }

                    EmitInstruction(new IRJmpIfEqZeroInstruction(Reg_Temp0, bodyInstructions.size() + offset), instructions);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    if (elseBodyInstructions.size() > 0)
                    {
                        EmitInstruction(new IRJmpInstruction(elseBodyInstructions.size() + 1), instructions);
                    }

                    for (auto& instruction : elseBodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }
                }
                else if (expression->GetType() == ASTNodeType::UnaryExpression)
                {
                    auto unaryExpression = (ASTUnaryExpression*)expression.get();
                    EmitUnaryExpression(unaryExpression, instructions, aliases);
                    EmitInstruction(new IRPopInstruction(Reg_Temp0), instructions);

                    auto offset = 1;
                    if (elseBodyInstructions.size() > 0)
                    {
                        offset = 2;
                    }

                    EmitInstruction(new IRJmpIfEqZeroInstruction(Reg_Temp0, bodyInstructions.size() + offset), instructions);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    if (elseBodyInstructions.size() > 0)
                    {
                        EmitInstruction(new IRJmpInstruction(elseBodyInstructions.size() + 1), instructions);
                    }

                    for (auto& instruction : elseBodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }
                }
                else if (expression->GetType() == ASTNodeType::FunctionCall)
                {
                    EmitFunctionCall((ASTFunctionCall*)expression.get(), instructions, aliases, false);

                    auto offset = 1;
                    if (elseBodyInstructions.size() > 0)
                    {
                        offset = 2;
                    }

                    EmitInstruction(new IRPopInstruction(Reg_Temp0), instructions);
                    EmitInstruction(new IRJmpIfEqZeroInstruction(Reg_Temp0, bodyInstructions.size() + offset), instructions);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    if (elseBodyInstructions.size() > 0)
                    {
                        EmitInstruction(new IRJmpInstruction(elseBodyInstructions.size() + 1), instructions);
                    }

                    for (auto& instruction : elseBodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }
                }
                else
                {
                    throw IRCompilerException("Unsupported type in expression");
                }
            }
            else if (statement->GetType() == ASTNodeType::WhileStatement)
            {
                auto whileStatement = (ASTWhileStatement*)statement.get();
                auto expression = whileStatement->GetExpression();
                auto body = whileStatement->GetBody();

                if (body->GetType() != ASTNodeType::BlockStatement)
                {
                    throw IRCompilerException("Invalid AST. While statement body must be a block statement");
                }

                if (expression->GetType() == ASTNodeType::NumberLiteral)
                {
                    auto numberLiteral = (ASTNumberLiteral*)expression.get();
                    if (numberLiteral->GetValue() > 0)
                    {
                        auto loopStart = instructions.size();
                        EmitBlockStatement((ASTBlockStatement*)body.get(), instructions, aliases);
                        EmitInstruction(new IRJmpInstruction(loopStart, true), instructions);
                    }
                }
                else if (expression->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)expression.get();

                    std::vector<std::unique_ptr<IIRInstruction>> bodyInstructions;
                    EmitBlockStatement((ASTBlockStatement*)body.get(), bodyInstructions, aliases);

                    if (bodyInstructions.size() == 0)
                    {
                        throw IRCompilerException("Disallowed while statement with empty body");
                    }

                    auto loopStart = instructions.size();
                    auto regId = RegisterNameToIndex(identifier->GetName(), aliases);
                    EmitInstruction(new IRJmpIfEqZeroInstruction(regId, bodyInstructions.size() + 2), instructions);

                    auto instructionCount = instructions.size();
                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    EmitInstruction(new IRJmpInstruction(loopStart, true), instructions);
                }
                else if (expression->GetType() == ASTNodeType::BinaryExpression)
                {
                    std::vector<std::unique_ptr<IIRInstruction>> bodyInstructions;
                    EmitBlockStatement((ASTBlockStatement*)body.get(), bodyInstructions, aliases);

                    if (bodyInstructions.size() == 0)
                    {
                        throw IRCompilerException("Disallowed while statement with empty body");
                    }

                    auto loopStart = instructions.size();
                    auto binaryExpression = (ASTBinaryExpression*)expression.get();
                    EmitBinaryExpression(binaryExpression, instructions, aliases);
                    EmitInstruction(new IRPopInstruction(Reg_Temp0), instructions);
                    EmitInstruction(new IRJmpIfEqZeroInstruction(Reg_Temp0, bodyInstructions.size() + 2), instructions);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    EmitInstruction(new IRJmpInstruction(loopStart, true), instructions);
                }
                else if (expression->GetType() == ASTNodeType::UnaryExpression)
                {
                    std::vector<std::unique_ptr<IIRInstruction>> bodyInstructions;
                    EmitBlockStatement((ASTBlockStatement*)body.get(), bodyInstructions, aliases);

                    if (bodyInstructions.size() == 0)
                    {
                        throw IRCompilerException("Disallowed while statement with empty body");
                    }

                    auto loopStart = instructions.size();
                    auto unaryExpression = (ASTUnaryExpression*)expression.get();
                    EmitUnaryExpression(unaryExpression, instructions, aliases);
                    EmitInstruction(new IRPopInstruction(Reg_Temp0), instructions);
                    EmitInstruction(new IRJmpIfEqZeroInstruction(Reg_Temp0, bodyInstructions.size() + 2), instructions);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    EmitInstruction(new IRJmpInstruction(loopStart, true), instructions);
                }
                else
                {
                    throw IRCompilerException("Unsupported type in expression");
                }
            }
            else if (statement->GetType() == ASTNodeType::ReturnStatement)
            {
                auto returnStatement = (ASTReturnStatement*)statement.get();
                if (returnStatement->GetExpression() != nullptr)
                {
                    auto& expression = returnStatement->GetExpression();

                    if (expression->GetType() == ASTNodeType::NumberLiteral)
                    {
                        auto number = (ASTNumberLiteral*)expression.get();
                        EmitInstruction(new IRPushInstruction(number->GetValue(), true), instructions);
                    }
                    else
                    {
                        EmitExpression(returnStatement->GetExpression().get(), instructions, aliases);
                    }
                }

                EmitInstruction(new IRJmpInstruction(JMP_TO_END_OFFSET_CONSTANT, true), instructions);
            }
            else
            {
                throw IRCompilerException("Unsupported statement type in function body");
            }

            statementIndex++;
        }

        for (auto& name : localVariables)
        {
            aliases.Deallocate(name);
        }

        return startIndex;
    }

    unsigned int IRCompiler::EmitFunction(ASTFunctionDeclaration* fn, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases)
    {
        auto body = fn->GetChild(0);
        if (body->GetType() != ASTNodeType::BlockStatement)
        {
            throw IRCompilerException("Invalid AST. Function body is not a block statement");
        }

        auto blockStatement = (ASTBlockStatement*)body.get();

        auto startIndex = instructions.size();

        auto argsCount = fn->GetArgumentCount();

        auto& args = fn->GetArguments();
        for (auto i = 0u; i < argsCount; i++)
        {
            auto& argName = args[i];
            auto regId = aliases.Allocate(argName);
            EmitInstruction(new IRPopInstruction(regId), instructions);
        }

        auto instructionsStart = instructions.size();
        EmitBlockStatement(blockStatement, instructions, aliases);

        auto endOffset = instructions.size();

        for (auto i = instructionsStart; i < instructions.size(); i++)
        {
            auto& instruction = instructions[i];
            if (instruction->GetType() == IRInstructionType::Jmp)
            {
                auto jmp = (IRJmpInstruction*)instruction.get();
                if (jmp->GetOffset() == JMP_TO_END_OFFSET_CONSTANT)
                {
                    jmp->SetOffset(endOffset + 1);
                }
            }
        }

        for (auto i = 0u; i < argsCount; i++)
        {
            auto& argName = args[i];
            aliases.Deallocate(argName);
        }

        if (fn->GetName() == "main")
        {
            if (instructions.back()->GetType() != IRInstructionType::Jmp)
            {
                EmitInstruction(new IRJmpInstruction(startIndex, true), instructions);
            }
        }
        else if (instructions.back()->GetType() != IRInstructionType::Push)
        {
            EmitInstruction(new IRPushInstruction(0, true), instructions);
        }

        return startIndex;
    }

    bool IRCompiler::IsRegisterName(const std::string& name, RegisterAliases& aliases) const
    {
        auto regId = aliases.GetAlias(name);
        if (regId == -1)
        {
            if (name.length() < 2 || name.length() > 4 || name[0] != 'r')
            {
                return false;
            }

            for (auto i = 1u; i < name.length(); i++)
            {
                if (!std::isdigit(name[i]))
                {
                    return false;
                }
            }

            return true;
        }

        return true;
    }

    int IRCompiler::RegisterNameToIndex(const std::string& name, RegisterAliases& aliases) const
    {
        auto regId = aliases.GetAlias(name);
        if (regId == -1)
        {
            std::string reg(name.c_str() + 1, name.length() - 1);
            auto index = std::atoi(reg.c_str());
            if (index == 0 || index >= MAX_REGISTER_INDEX)
            {
                throw IRCompilerException(SafePrintf("Invalid register \"%\"", name));
            }

            return index;
        }

        return regId;
    }

    int IRCompiler::UnitNameToId(const std::string& name) const
    {
        for (auto i = 0; i < std::extent<decltype(CHK::UnitsByName)>::value; i++)
        {
            if (name == CHK::UnitsByName[i])
            {
                return i;
            }
        }

        return -1;
    }

    int IRCompiler::PlayerNameToId(const std::string& name) const
    {
        for (auto i = 0; i < std::extent<decltype(CHK::PlayersByName)>::value; i++)
        {
            if (name == CHK::PlayersByName[i])
            {
                return i;
            }
        }

        return -1;
    }

}
