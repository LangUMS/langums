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
        m_WavFilenames.clear();

        auto aliases = RegisterAliases();

        if (ast->GetType() != ASTNodeType::Unit)
        {
            throw IRCompilerException("Invalid AST node type, expected Unit", ast.get());
        }

        m_Unit = ast.get();

        auto& unitNodes = ast->GetChildren();
        m_EventCount = 0;
        for (auto& node : unitNodes)
        {
            if (node->GetType() == ASTNodeType::EventDeclaration)
            {
                m_EventCount++;
            }
        }

        m_DebugStackFrames.clear();

        for (auto& node : unitNodes)
        {
            if (node->GetType() == ASTNodeType::VariableDeclaration)
            {
                auto variable = (ASTVariableDeclaration*)node.get();
                auto& name = variable->GetName();

                if (aliases.HasAlias(name, 0, node.get()))
                {
                    throw IRCompilerException(SafePrintf("Duplicate global variable declaration \"%\"", name), node.get());
                }

                auto arraySize = variable->GetArraySize();
                aliases.Allocate(name, arraySize, node.get());

                auto& expression = variable->GetExpression();

                if (expression != nullptr)
                {
                    if (expression->GetType() != ASTNodeType::NumberLiteral)
                    {
                        throw IRCompilerException(SafePrintf("Trying to initialize global \"%\" with something other than a number literal", name), node.get());
                    }

                    auto number = (ASTNumberLiteral*)expression.get();
                    auto value = number->GetValue();

                    for (auto i = 0u; i < arraySize; i++)
                    {
                        auto regId = aliases.GetAlias(name, i, expression.get());
                        EmitInstruction(new IRSetRegInstruction(regId, value), m_Instructions, expression.get(), aliases);
                    }
                }
            }
        }

        auto nextUnitSlot = 0;

        for (auto& node : unitNodes)
        {
            if (node->GetType() == ASTNodeType::FunctionDeclaration)
            {
                auto fn = (ASTFunctionDeclaration*)node.get();
                auto& name = fn->GetName();

                if (m_FunctionDeclarations.find(name) != m_FunctionDeclarations.end())
                {
                    throw IRCompilerException(SafePrintf("Duplicate function declaration \"%\"", name), node.get());
                }

                m_FunctionDeclarations.insert(std::make_pair(name, fn));
            }
            else if (node->GetType() == ASTNodeType::UnitProperties)
            {
                auto unitProps = (ASTUnitProperties*)node.get();
                auto& name = unitProps->GetName();

                if (m_UnitProperties.find(name) != m_UnitProperties.end())
                {
                    throw IRCompilerException(SafePrintf("Duplicate unit properties declaration \"%\"", name), node.get());
                }

                m_UnitProperties.insert(std::make_pair(name, nextUnitSlot++));

                auto propsCount = unitProps->GetPropertiesCount();
                EmitInstruction(new IRUnitInstruction(propsCount), m_Instructions, node.get(), aliases);

                for (auto i = 0u; i < propsCount; i++)
                {
                    auto unitProp = (ASTUnitProperty*)unitProps->GetProperty(i).get();
                    auto propType = ParseUnitPropType(unitProp->GetName(), unitProp);

                    EmitInstruction(new IRUnitPropInstruction(propType, unitProp->GetValue()), m_Instructions, unitProp, aliases);
                }
            }
        }

        if (m_FunctionDeclarations.find("main") == m_FunctionDeclarations.end())
        {
            throw IRCompilerException("main() function not found", 0);
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
                    throw IRCompilerException("Maximum event conditions reached (63 conditions per event)", node.get());
                }

                EmitInstruction(new IREventInstruction(nextSwitchId++, conditionsCount), m_Instructions, node.get(), aliases);

                for (auto i = 0u; i < conditionsCount; i++)
                {
                    auto condition = (ASTEventCondition*)eventDeclaration->GetCondition(i).get();
                    auto& name = condition->GetName();

                    if (name == "value")
                    {
                        unsigned int regId = 0;

                        auto arg0 = condition->GetArgument(0);
                        if (arg0->GetType() == ASTNodeType::Identifier)
                        {
                            auto identifier = (ASTIdentifier*)arg0.get();
                            regId = aliases.GetAlias(identifier->GetName(), 0, identifier);
                        }
                        else if (arg0->GetType() == ASTNodeType::ArrayExpression)
                        {
                            auto arrayExpression = (ASTArrayExpression*)arg0.get();
                            auto index = arrayExpression->GetIndex();
                            if (index->GetType() != ASTNodeType::NumberLiteral)
                            {
                                throw IRCompilerException(SafePrintf("Invalid index for array expression in argument 0 in call to \"%\", expected number literal", name), condition);
                            }

                            auto arrayIndex = (ASTNumberLiteral*)index.get();
                            regId = aliases.GetAlias(arrayExpression->GetIdentifier(), arrayIndex->GetValue(), arrayExpression);
                        }
                        else
                        {
                            throw IRCompilerException(SafePrintf("Invalid argument type for argument 0 in call to \"%\", expected global variable name", name), condition);
                        }

                        auto comparison = ParseComparisonArgument(condition->GetArgument(1), name, 1);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(2), name, 2);

                        EmitInstruction(new IRRegCondInstruction(regId, comparison, quantity), m_Instructions, condition, aliases);
                    }
                    else if (name == "bring")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto comparison = ParseComparisonArgument(condition->GetArgument(1), name, 1);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(2), name, 2);
                        auto unitId = ParseUnitTypeArgument(condition->GetArgument(3), name, 3);
                        auto locationName = ParseLocationArgument(condition->GetArgument(4), name, 4);

                        EmitInstruction(new IRBringCondInstruction(playerId, unitId, locationName, comparison, quantity), m_Instructions, condition, aliases);
                    }
                    else if (name == "commands" || name == "killed" || name == "deaths")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto comparison = ParseComparisonArgument(condition->GetArgument(1), name, 1);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(2), name, 2);
                        auto unitId = ParseUnitTypeArgument(condition->GetArgument(3), name, 3);

                        if (name == "commands")
                        {
                            EmitInstruction(new IRCmdCondInstruction(playerId, unitId, comparison, quantity), m_Instructions, condition, aliases);
                        }
                        else if (name == "killed")
                        {
                            EmitInstruction(new IRKillCondInstruction(playerId, unitId, comparison, quantity), m_Instructions, condition, aliases);
                        }
                        else if (name == "deaths")
                        {
                            EmitInstruction(new IRDeathCondInstruction(playerId, unitId, comparison, quantity), m_Instructions, condition, aliases);
                        }
                    }
                    else if (name == "commands_least" || name == "commands_most")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto unitId = ParseUnitTypeArgument(condition->GetArgument(1), name, 1);

                        std::string locationName;

                        if (condition->GetChildCount() > 2)
                        {
                            locationName = ParseLocationArgument(condition->GetArgument(2), name, 2);
                        }

                        if (name == "commands_least")
                        {
                            EmitInstruction(new IRCmdLeastCondInstruction(playerId, unitId, locationName), m_Instructions, condition, aliases);
                        }
                        else if (name == "commands_most")
                        {
                            EmitInstruction(new IRCmdMostCondInstruction(playerId, unitId, locationName), m_Instructions, condition, aliases);
                        }
                    }
                    else if (name == "killed_least" || name == "killed_most")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto unitId = ParseUnitTypeArgument(condition->GetArgument(1), name, 1);

                        if (name == "killed_least")
                        {
                            EmitInstruction(new IRKillLeastCondInstruction(playerId, unitId), m_Instructions, condition, aliases);
                        }
                        else if (name == "killed_most")
                        {
                            EmitInstruction(new IRKillMostCondInstruction(playerId, unitId), m_Instructions, condition, aliases);
                        }
                    }
                    else if (name == "accumulate")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto comparison = ParseComparisonArgument(condition->GetArgument(1), name, 1);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(2), name, 2);
                        auto resType = ParseResourceTypeArgument(condition->GetArgument(3), name, 3);

                        EmitInstruction(new IRAccumCondInstruction(playerId, resType, comparison, quantity), m_Instructions, condition, aliases);
                    }
                    else if (name == "least_resources" || name == "most_resources")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto resType = ParseResourceTypeArgument(condition->GetArgument(1), name, 1);

                        if (name == "least_resources")
                        {
                            EmitInstruction(new IRLeastResCondInstruction(playerId, resType), m_Instructions, condition, aliases);
                        }
                        else if (name == "most_resources")
                        {
                            EmitInstruction(new IRMostResCondInstruction(playerId, resType), m_Instructions, condition, aliases);
                        }
                    }
                    else if (name == "score")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto scoreType = ParseScoreTypeArgument(condition->GetArgument(1), name, 1);
                        auto comparison = ParseComparisonArgument(condition->GetArgument(2), name, 2);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(3), name, 3);

                        EmitInstruction(new IRScoreCondInstruction(playerId, scoreType, comparison, quantity), m_Instructions, condition, aliases);
                    }
                    else if (name == "lowest_score" || name == "highest_score")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto scoreType = ParseScoreTypeArgument(condition->GetArgument(1), name, 1);

                        if (name == "lowest_score")
                        {
                            EmitInstruction(new IRLowScoreCondInstruction(playerId, scoreType), m_Instructions, condition, aliases);
                        }
                        else if (name == "highest_score")
                        {
                            EmitInstruction(new IRHiScoreCondInstruction(playerId, scoreType), m_Instructions, condition, aliases);
                        }
                    }
                    else if (name == "elapsed_time")
                    {
                        auto comparison = ParseComparisonArgument(condition->GetArgument(0), name, 0);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(1), name, 1);

                        EmitInstruction(new IRTimeCondInstruction(comparison, quantity), m_Instructions, condition, aliases);
                    }
                    else if (name == "countdown")
                    {
                        auto comparison = ParseComparisonArgument(condition->GetArgument(0), name, 0);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(1), name, 1);

                        EmitInstruction(new IRCountdownCondInstruction(comparison, quantity), m_Instructions, condition, aliases);
                    }
                    else if (name == "opponents")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto comparison = ParseComparisonArgument(condition->GetArgument(1), name, 1);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(2), name, 2);

                        EmitInstruction(new IROpponentsCondInstruction(playerId, comparison, quantity), m_Instructions, condition, aliases);
                    }
                    else
                    {
                        throw IRCompilerException(SafePrintf("Unknown condition type \"%\"", name), condition);
                    }
                }
            }
        }

        EmitInstruction(new IRChkPlayers(), m_Instructions, nullptr, aliases);

        auto main = m_FunctionDeclarations["main"];
        EmitFunction(main, m_Instructions, aliases);
        return true;
    }

    void IRCompiler::Optimize()
    {
        IROptimizer optimizer;
        m_Instructions = optimizer.Process(std::move(m_Instructions));
    }

    std::string IRCompiler::DumpInstructions(bool lineNumbers) const
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

    void IRCompiler::EmitInstruction(IIRInstruction* instruction, std::vector<std::unique_ptr<IIRInstruction>>& instructions, IASTNode* node, RegisterAliases& aliases)
    {
        instruction->SetASTNode(node);

        auto stackFrames = m_DebugStackFrames;

        for (auto& frame : stackFrames)
        {
            for (auto& global : aliases.GetAliases(node))
            {
                auto regId = aliases.GetAlias(global.first, 0, node);
                frame->m_Variables.push_back(std::make_pair(regId, global.first));
            }
        }

        instruction->SetDebugStackFrames(stackFrames);

        instructions.push_back(std::unique_ptr<IIRInstruction>(instruction));
    }

    void IRCompiler::EmitFunctionCall(ASTFunctionCall* fnCall, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool ignoreReturnValue)
    {
        auto stackFrame = std::make_shared<StackFrame>();
        stackFrame->m_FunctionName = fnCall->GetFunctionName();
        stackFrame->m_ASTNode = fnCall;
        m_DebugStackFrames.push_back(stackFrame);

        auto& fnName = fnCall->GetFunctionName();

        if (fnName == "poll_events")
        {
            EmitInstruction(new IRChkPlayers(), instructions, nullptr, aliases);
            EmitInstruction(new IRSetSwInstruction(Switch_EventsMutex, true), instructions, fnCall, aliases);

            auto nextSwitchId = (int)Switch_ReservedEnd;

            auto& nodes = m_Unit->GetChildren();
            for (auto& node : nodes)
            {
                if (node->GetType() != ASTNodeType::EventDeclaration)
                {
                    continue;
                }

                auto frame = std::make_shared<StackFrame>();
                frame->m_ASTNode = fnCall;
                frame->m_FunctionName = "EventHandler";
                m_DebugStackFrames.push_back(frame);

                auto eventDeclaration = (ASTEventDeclaration*)node.get();
                auto body = eventDeclaration->GetBody();
                if (body->GetType() != ASTNodeType::BlockStatement)
                {
                    throw IRCompilerException("Event body must be a block statement", node.get());
                }

                std::vector<std::unique_ptr<IIRInstruction>> bodyInstructions;
                EmitBlockStatement((ASTBlockStatement*)body.get(), bodyInstructions, aliases);

                for (auto i = 0u; i < bodyInstructions.size(); i++)
                {
                    auto& instruction = bodyInstructions[i];
                    if (instruction->GetType() == IRInstructionType::Jmp)
                    {
                        auto jmp = (IRJmpInstruction*)instruction.get();
                        if (jmp->GetOffset() == JMP_TO_END_OFFSET_CONSTANT)
                        {
                            jmp->SetOffset(bodyInstructions.size() - i);
                        }
                    }
                }

                auto switchId = nextSwitchId++;
                EmitInstruction(new IRJmpIfSwNotSetInstruction(switchId, bodyInstructions.size() + 1), instructions, node.get(), aliases);

                for (auto& instruction : bodyInstructions)
                {
                    instructions.push_back(std::move(instruction));
                }

                EmitInstruction(new IRSetSwInstruction(switchId, false), instructions, node.get(), aliases);

                m_DebugStackFrames.pop_back();
            }

            EmitInstruction(new IRSetSwInstruction(Switch_EventsMutex, false), instructions, fnCall, aliases);
        }
        else if (fnName == "clear_buffered_events")
        {
            for (auto i = 0u; i < m_EventCount; i++)
            {
                EmitInstruction(new IRSetSwInstruction((int)Reg_ReservedEnd + i, false), instructions, fnCall, aliases);
            }
        }
        else if (fnName == "debugger")
        {
            EmitInstruction(new IRDebugBrkInstruction(), instructions, fnCall, aliases);
        }
        else if (fnName == "is_present")
        {
            auto isPresent = new IRIsPresentInstruction();

            auto argCount = fnCall->GetChildCount();

            if (argCount == 0)
            {
                for (auto i = 0u; i < 8; i++)
                {
                    isPresent->AddPlayerId(i);
                }
            }
            else
            {
                for (auto i = 0u; i < argCount; i++)
                {
                    auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(i), fnName, i);
                    isPresent->AddPlayerId(playerId);
                }
            }

            EmitInstruction(isPresent, instructions, fnCall, aliases);
        }
        else if (fnName == "rnd256" || fnName == "random")
        {
            if (!ignoreReturnValue)
            {
                EmitInstruction(new IRRnd256Instruction(), instructions, fnCall, aliases);
            }
        }
        else if (fnName == "set_vision")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_vision() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("set_vision() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto targetPlayerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            auto state = ParseQuantityArgument(fnCall->GetArgument(2), fnName, 2);

            std::string scriptName;
            if (state == 0)
            {
                scriptName = SafePrintf("-Vi%", (int)targetPlayerId);
            }
            else
            {
                scriptName = SafePrintf("+Vi%", (int)targetPlayerId);
            }

            unsigned int scriptId;
            memcpy(&scriptId, scriptName.data(), 4);

            EmitInstruction(new IRAIScriptInstruction(playerId, scriptId, "AnyLocation"), instructions, fnCall, aliases);
        }
        else if (fnName == "end")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("end() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 2)
            {
                throw IRCompilerException("end() takes exactly two arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto endCondition = ParseEndGameCondition(fnCall->GetArgument(1), fnName, 1);

            EmitInstruction(new IREndGameInstruction(endCondition, playerId), instructions, fnCall, aliases);
        }
        else if (fnName == "set_resource")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_resource() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("set_resource() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto resType = ParseResourceTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRSetResourceInstruction(playerId, resType, regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "add_resource")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("add_resource() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("add_resource() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto resType = ParseResourceTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRIncResourceInstruction(playerId, resType, regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "take_resource")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("take_resource() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("take_resource() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto resType = ParseResourceTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRDecResourceInstruction(playerId, resType, regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "set_score")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_score() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("set_score() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto scoreType = ParseScoreTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRSetScoreInstruction(playerId, scoreType, regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "add_score")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("add_score() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("add_score() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto scoreType = ParseScoreTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRIncScoreInstruction(playerId, scoreType, regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "subtract_score")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("subtract_score() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("subtract_score() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto scoreType = ParseScoreTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRDecScoreInstruction(playerId, scoreType, regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "set_score")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_score() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("set_score() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto scoreType = ParseScoreTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRSetScoreInstruction(playerId, scoreType, regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "set_countdown")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_countdown() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 1)
            {
                throw IRCompilerException("set_countdown() takes exactly one argument", fnCall);
            }

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(0), fnName, 0, instructions, aliases, isLiteral);

            EmitInstruction(new IRSetCountdownInstruction(regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "add_countdown")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("add_countdown() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 1)
            {
                throw IRCompilerException("add_countdown() takes exactly one argument", fnCall);
            }

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(0), fnName, 0, instructions, aliases, isLiteral);

            EmitInstruction(new IRAddCountdownInstruction(regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "sub_countdown")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("sub_countdown() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 1)
            {
                throw IRCompilerException("sub_countdown() takes exactly one argument", fnCall);
            }

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(0), fnName, 0, instructions, aliases, isLiteral);

            EmitInstruction(new IRSubCountdownInstruction(regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "pause_countdown()")
        {
            EmitInstruction(new IRPauseCountdownInstruction(false), instructions, fnCall, aliases);
        }
        else if (fnName == "unpause_countdown()")
        {
            EmitInstruction(new IRPauseCountdownInstruction(true), instructions, fnCall, aliases);
        }
        else if (fnName == "mute_unit_speech()")
        {
            EmitInstruction(new IRMuteUnitSpeechInstruction(false), instructions, fnCall, aliases);
        }
        else if (fnName == "unmute_unit_speech()")
        {
            EmitInstruction(new IRMuteUnitSpeechInstruction(true), instructions, fnCall, aliases);
        }
        else if (fnName == "set_deaths")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_deaths() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("set_deaths() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRSetDeathsInstruction(playerId, unitId, regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "add_deaths")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("inc_deaths() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("inc_deaths() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRIncDeathsInstruction(playerId, unitId, regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "remove_deaths")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("remove_deaths() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("remove_deaths() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRDecDeathsInstruction(playerId, unitId, regId, isLiteral), instructions, fnCall, aliases);
        }
        else if (fnName == "talking_portrait")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("talking_portrait() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("talking_portrait() takes exactly three arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);
            auto time = ParseQuantityArgument(fnCall->GetArgument(2), fnName, 2);
            EmitInstruction(new IRTalkInstruction(playerId, unitId, time * 1000), instructions, fnCall, aliases);
        }
        else if (fnName == "set_doodad")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_doodad() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 4)
            {
                throw IRCompilerException("set_doodad() takes exactly four arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);
            auto state = ParseToggleState(fnCall->GetArgument(2), fnName, 2);
            auto locationName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);

            EmitInstruction(new IRSetDoodadInstruction(playerId, unitId, locationName, state), instructions, fnCall, aliases);
        }
        else if (fnName == "set_invincibility")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_invincibility() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 4)
            {
                throw IRCompilerException("set_invincibility() takes exactly four arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);
            auto state = ParseToggleState(fnCall->GetArgument(2), fnName, 2);
            auto locationName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);

            EmitInstruction(new IRSetInvincibleInstruction(playerId, unitId, locationName, state), instructions, fnCall, aliases);
        }
        else if (fnName == "run_ai_script")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("run_ai_script() called without arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto scriptName = ParseAIScriptArgument(fnCall->GetArgument(1), fnName, 1);

            std::string locationName;
            if (fnCall->GetChildCount() > 2)
            {
                locationName = ParseLocationArgument(fnCall->GetArgument(2), fnName, 2);
            }

            EmitInstruction(new IRAIScriptInstruction(playerId, scriptName, locationName), instructions, fnCall, aliases);
        }
        else if (fnName == "set_alliance")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_alliance() called without arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto targetPlayerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            auto status = ParseAllianceStatusArgument(fnCall->GetArgument(2), fnName, 2);

            EmitInstruction(new IRSetAllyInstruction(playerId, targetPlayerId, status), instructions, fnCall, aliases);
        }
        else if (fnName == "set_mission_objectives")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_mission_objectives() called without arguments", fnCall);
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException("set_mission_objectives() accepts a single string literal argument", fnCall);
            }

            auto playerId = -1;

            if (fnCall->GetChildCount() > 1)
            {
                playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            }

            auto stringLiteral = (ASTStringLiteral*)arg0.get();

            EmitInstruction(new IRSetObjInstruction(playerId, stringLiteral->GetValue()), instructions, fnCall, aliases);
        }
        else if (fnName == "pause_game" || fnName == "unpause_game")
        {
            EmitInstruction(new IRPauseGameInstruction(fnName == "unpause_game"), instructions, fnCall, aliases);
        }
        else if (fnName == "set_next_scenario")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_next_scenario() called without arguments", fnCall);
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException("set_next_scenario() accepts a single string literal argument", fnCall);
            }

            auto stringLiteral = (ASTStringLiteral*)arg0.get();

            EmitInstruction(new IRNextScenInstruction(stringLiteral->GetValue()), instructions, fnCall, aliases);
        }
        else if (fnName == "show_leaderboard")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("show_leaderboard() called without arguments", fnCall);
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Invalid argument type for argument 1 in call to \"%\", expected string literal", fnName), fnCall);
            }

            auto& text = ((ASTStringLiteral*)arg0.get())->GetValue();
            auto type = ParseLeaderboardType(fnCall->GetArgument(1), fnName, 1);

            auto showLeaderboard = new IRLeaderboardInstruction(text, type, -1);

            if (type == LeaderboardType::Control)
            {
                auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(2), fnName, 2);
                showLeaderboard->SetUnitId(unitId);
            }
            else if (type == LeaderboardType::ControlAtLocation)
            {
                auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(2), fnName, 2);
                showLeaderboard->SetUnitId(unitId);

                auto locationName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);
                showLeaderboard->SetLocationName(locationName);
            }
            else if (type == LeaderboardType::Kills)
            {
                auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(2), fnName, 2);
                showLeaderboard->SetUnitId(unitId);
            }
            else if (type == LeaderboardType::Points)
            {
                auto scoreType = ParseScoreTypeArgument(fnCall->GetArgument(2), fnName, 2);
                showLeaderboard->SetScoreType(scoreType);
            }
            else if (type == LeaderboardType::Resources)
            {
                auto resourceType = ParseResourceTypeArgument(fnCall->GetArgument(2), fnName, 2);
                showLeaderboard->SetResourceType(resourceType);
            }

            EmitInstruction(showLeaderboard, instructions, fnCall, aliases);
        }
        else if (fnName == "show_leaderboard_goal")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("show_leaderboard_goal() called without arguments", fnCall);
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Invalid argument type for argument 1 in call to \"%\", expected string literal", fnName), fnCall);
            }

            auto& text = ((ASTStringLiteral*)arg0.get())->GetValue();
            auto type = ParseLeaderboardType(fnCall->GetArgument(1), fnName, 1);
            auto goalQuantity = ParseQuantityArgument(fnCall->GetArgument(2), fnName, 2);

            auto showLeaderboard = new IRLeaderboardInstruction(text, type, goalQuantity);

            if (type == LeaderboardType::Control)
            {
                auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(3), fnName, 3);
                showLeaderboard->SetUnitId(unitId);
            }
            else if (type == LeaderboardType::ControlAtLocation)
            {
                auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(3), fnName, 3);
                showLeaderboard->SetUnitId(unitId);

                auto locationName = ParseLocationArgument(fnCall->GetArgument(4), fnName, 4);
                showLeaderboard->SetLocationName(locationName);
            }
            else if (type == LeaderboardType::Kills)
            {
                auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(3), fnName, 3);
                showLeaderboard->SetUnitId(unitId);
            }
            else if (type == LeaderboardType::Points)
            {
                auto scoreType = ParseScoreTypeArgument(fnCall->GetArgument(3), fnName, 3);
                showLeaderboard->SetScoreType(scoreType);
            }
            else if (type == LeaderboardType::Resources)
            {
                auto resourceType = ParseResourceTypeArgument(fnCall->GetArgument(3), fnName, 3);
                showLeaderboard->SetResourceType(resourceType);
            }

            EmitInstruction(showLeaderboard, instructions, fnCall, aliases);
        }
        else if (fnName == "leaderboard_show_cpu")
        {
            if (!fnCall->HasChildren())
            {
                EmitInstruction(new IRLeaderboardCpuInstruction(TriggerActionState::SetSwitch), instructions, fnCall, aliases);
            }
            else
            {
                auto state = ParseToggleState(fnCall->GetArgument(0), fnName, 0);
                EmitInstruction(new IRLeaderboardCpuInstruction(state), instructions, fnCall, aliases);
            }
        }
        else if (fnName == "center_view")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("center_view() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 2)
            {
                throw IRCompilerException("center_view() takes exactly two arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto locName = ParseLocationArgument(fnCall->GetArgument(1), fnName, 1);

            EmitInstruction(new IRCenterViewInstruction(playerId, locName), instructions, fnCall, aliases);
        }
        else if (fnName == "ping")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("ping() called without arguments", fnCall);
            }

            if (fnCall->GetChildCount() != 2)
            {
                throw IRCompilerException("ping() takes exactly two arguments", fnCall);
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto locName = ParseLocationArgument(fnCall->GetArgument(1), fnName, 1);

            EmitInstruction(new IRPingInstruction(playerId, locName), instructions, fnCall, aliases);
        }
        else if (fnName == "print")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("print() called without arguments", fnCall);
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal passed as first argument to %()", fnName), fnCall);
            }

            auto msg = (ASTStringLiteral*)arg0.get();
            auto playerId = -1;

            if (fnCall->GetChildCount() >= 2)
            {
                playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            }

            EmitInstruction(new IRDisplayMsgInstruction(msg->GetValue(), playerId), instructions, fnCall, aliases);
        }
        else if (fnName == "sleep")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("sleep() called without arguments", fnCall);
            }

            auto quantity = ParseQuantityArgument(fnCall->GetArgument(0), fnName, 0);
            EmitInstruction(new IRWaitInstruction(quantity), instructions, fnCall, aliases);
        }
        else if (fnName == "spawn" || fnName == "kill" || fnName == "remove")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName), fnCall);
            }

            if (fnName == "spawn" && (fnCall->GetChildCount() < 4 || fnCall->GetChildCount() > 5))
            {
                throw IRCompilerException(SafePrintf("%() takes 4 or 5 arguments", fnName), fnCall);
            }
            else if (fnName == "kill" && (fnCall->GetChildCount() < 3 || fnCall->GetChildCount() > 4))
            {
                throw IRCompilerException(SafePrintf("%() takes 3 or 4 arguments", fnName), fnCall);
            }
            else if (fnName == "remove" && (fnCall->GetChildCount() < 3 || fnCall->GetChildCount() > 4))
            {
                throw IRCompilerException(SafePrintf("%() takes 3 or 4 arguments", fnName), fnCall);
            }

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            std::string locName;
            if (fnCall->GetChildCount() >= 4)
            {
                locName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);
            }

            if (fnName == "spawn")
            {
                auto propsSlot = -1;
                if (fnCall->GetChildCount() == 5)
                {
                    auto arg4 = fnCall->GetArgument(4);
                    if (arg4->GetType() != ASTNodeType::Identifier)
                    {
                        throw IRCompilerException(SafePrintf("Invalid argument type for argument 5 in call to \"%\", expected unit slot name", fnName), fnCall);
                    }

                    auto& slotName = ((ASTIdentifier*)arg4.get())->GetName();

                    if (m_UnitProperties.find(slotName) == m_UnitProperties.end())
                    {
                        throw IRCompilerException(SafePrintf("Invalid argument value \"%\" for argument 5 in call to \"%\", slot with such name does not exist", slotName, fnName), fnCall);
                    }

                    propsSlot = m_UnitProperties[slotName];
                }

                EmitInstruction(new IRSpawnInstruction(playerId, unitId, regId, isLiteral, locName, propsSlot), instructions, fnCall, aliases);
            }
            else if (fnName == "kill")
            {
                EmitInstruction(new IRKillInstruction(playerId, unitId, regId, isLiteral, locName), instructions, fnCall, aliases);
            }
            else if (fnName == "remove")
            {
                EmitInstruction(new IRRemoveInstruction(playerId, unitId, regId, isLiteral, locName), instructions, fnCall, aliases);
            }
        }
        else if (fnName == "move")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName), fnCall);
            }

            if (fnCall->GetChildCount() != 5)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 5 arguments", fnName), fnCall);
            }

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            auto srcLocName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);
            auto dstLocName = ParseLocationArgument(fnCall->GetArgument(4), fnName, 4);

            EmitInstruction(new IRMoveInstruction(playerId, unitId, regId, isLiteral, srcLocName, dstLocName), instructions, fnCall, aliases);
        }
        else if (fnName == "order")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName), fnCall);
            }

            if (fnCall->GetChildCount() != 5)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 5 arguments", fnName), fnCall);
            }

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            auto order = ParseOrderType(fnCall->GetArgument(2), fnName, 2);
            auto srcLocName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);
            auto dstLocName = ParseLocationArgument(fnCall->GetArgument(4), fnName, 4);

            EmitInstruction(new IROrderInstruction(playerId, unitId, order, srcLocName, dstLocName), instructions, fnCall, aliases);
        }
        else if (fnName == "modify")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName), fnCall);
            }

            if (fnCall->GetChildCount() != 6)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 6 arguments", fnName), fnCall);
            }

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            auto arg3 = fnCall->GetArgument(3);
            if (arg3->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as third argument to %(), expected Health, Energy, Shields or Hangar", fnName), fnCall);
            }

            auto modifyType = ParseModifyType(fnCall->GetArgument(3), fnName, 3);
            auto amount = ParseQuantityArgument(fnCall->GetArgument(4), fnName, 4);
            auto locName = ParseLocationArgument(fnCall->GetArgument(5), fnName, 5);

            EmitInstruction(new IRModifyInstruction(playerId, unitId, regId, isLiteral, amount, modifyType, locName), instructions, fnCall, aliases);
        }
        else if (fnName == "give")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName), fnCall);
            }

            if (fnCall->GetChildCount() != 5)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 5 arguments", fnName), fnCall);
            }

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto srcPlayerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            auto dstPlayerId = ParsePlayerIdArgument(fnCall->GetArgument(2), fnName, 2);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(3), fnName, 3, instructions, aliases, isLiteral);

            auto locName = ParseLocationArgument(fnCall->GetArgument(4), fnName, 4);

            EmitInstruction(new IRGiveInstruction(srcPlayerId, dstPlayerId, unitId, regId, isLiteral, locName), instructions, fnCall, aliases);
        }
        else if (fnName == "move_loc")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName), fnCall);
            }

            if (fnCall->GetChildCount() != 4)
            {
                throw IRCompilerException(SafePrintf("%() takes exactly 4 arguments", fnName), fnCall);
            }

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            auto srcLocName = ParseLocationArgument(fnCall->GetArgument(2), fnName, 2);
            auto dstLocName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);

            EmitInstruction(new IRMoveLocInstruction(playerId, unitId, srcLocName, dstLocName), instructions, fnCall, aliases);
        }
        else if (fnName == "play_sound")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException(SafePrintf("%() called without arguments", fnName), fnCall);
            }

            auto arg0 = fnCall->GetArgument(0);
            if (arg0->GetType() != ASTNodeType::StringLiteral)
            {
                throw IRCompilerException(SafePrintf("Invalid first argument passed to play_sound(), expected .wav filename", fnName), fnCall);
            }

            auto& wavFilename = ((ASTStringLiteral*)arg0.get())->GetValue();
            m_WavFilenames.insert(wavFilename);

            auto playerId = -1;
            if (fnCall->GetChildCount() > 1)
            {
                playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            }

            EmitInstruction(new IRPlayWAVInstruction(playerId, wavFilename, 0), instructions, fnCall, aliases);
        }
        else if (m_FunctionDeclarations.find(fnName) != m_FunctionDeclarations.end())
        {
            auto declaration = m_FunctionDeclarations[fnName];
            auto argCount = declaration->GetArgumentCount();
            auto& argNames = declaration->GetArguments();

            if (fnCall->GetChildCount() != argCount)
            {
                throw IRCompilerException(SafePrintf("Argument count mismatch when calling function \"%\", got % but expected %",
                    fnName, fnCall->GetChildCount(), argCount), fnCall);
            }

            for (auto i = 0u; i < argCount; i++)
            {
                auto& arg = fnCall->GetArgument(i);
                EmitExpression(arg.get(), instructions, aliases);
            }

            EmitFunction(declaration, instructions, aliases);

            auto hasReturnValue = instructions.back()->GetType() == IRInstructionType::Push;
            if (hasReturnValue && ignoreReturnValue)
            {
                EmitInstruction(new IRPopInstruction(), instructions, fnCall, aliases);
            }
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid function name \"%\"", fnName), fnCall);
        }

        m_DebugStackFrames.pop_back();
    }

    void IRCompiler::EmitBinaryExpression(ASTBinaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases)
    {
        auto& rhs = expression->GetLHSValue();
        auto& lhs = expression->GetRHSValue();

        auto op = expression->GetOperator();
        if (op == OperatorType::Add)
        {
            EmitExpression(rhs.get(), instructions, aliases);
            EmitExpression(lhs.get(), instructions, aliases);
            EmitInstruction(new IRAddInstruction(), instructions, expression, aliases);
        }
        else if (op == OperatorType::Subtract)
        {
            EmitExpression(rhs.get(), instructions, aliases);
            EmitExpression(lhs.get(), instructions, aliases);
            EmitInstruction(new IRSubInstruction(), instructions, expression, aliases);
        }
        else if (op == OperatorType::Multiply)
        {
            if (lhs->GetType() == ASTNodeType::NumberLiteral || rhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto number = (ASTNumberLiteral*)((lhs->GetType() == ASTNodeType::NumberLiteral) ? lhs.get() : rhs.get());
                auto value = number->GetValue();

                if (value == 0)
                {
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    return;
                }

                if (value == 1)
                {
                    EmitExpression(lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get(), instructions, aliases);
                    return;
                }

                if (value == 2) // multiplication by 2 is converted to an addition
                {
                    EmitExpression(lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get(), instructions, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRCopyRegInstruction(Reg_StackTop, Reg_StackTop + 1), instructions, expression, aliases);
                    EmitInstruction(new IRAddInstruction(), instructions, expression, aliases);
                    return;
                }

                EmitExpression(lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get(), instructions, aliases);
                EmitInstruction(new IRMulConstInstruction(value), instructions, expression, aliases);
                return;
            }

            EmitExpression(rhs.get(), instructions, aliases);
            EmitExpression(lhs.get(), instructions, aliases);
            EmitInstruction(new IRMulInstruction(), instructions, expression, aliases);
        }
        else if (op == OperatorType::Divide)
        {
            // very suboptimal division
            // TODO implement the Div instruction

            EmitExpression(lhs.get(), instructions, aliases);
            EmitExpression(rhs.get(), instructions, aliases);

            EmitInstruction(new IRSetRegInstruction(Reg_Temp1, 0), instructions, expression, aliases);
            EmitInstruction(new IRPopInstruction(Reg_Temp0), instructions, expression, aliases);
            EmitInstruction(new IRPushInstruction(Reg_Temp0), instructions, expression, aliases);
            EmitInstruction(new IRIncRegInstruction(Reg_Temp1, 1), instructions, expression, aliases);
            EmitInstruction(new IRSubInstruction(), instructions, expression, aliases);
            EmitInstruction(new IRJmpIfSwNotSetInstruction(Switch_ArithmeticUnderflow, -3), instructions, expression, aliases);
            EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
            EmitInstruction(new IRDecRegInstruction(Reg_Temp1, 1), instructions, expression, aliases);
            EmitInstruction(new IRPushInstruction(Reg_Temp1), instructions, expression, aliases);
        }
        else if (op == OperatorType::Equals)
        {
            if (lhs->GetType() == ASTNodeType::NumberLiteral || rhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)(lhs->GetType() == ASTNodeType::NumberLiteral ? lhs.get() : rhs.get());
                auto value = numberLiteral->GetValue();

                auto other = lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get();
                auto regId = (int)Reg_StackTop;

                if (other->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)other;
                    regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (other->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)other;
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }

                if (regId == Reg_StackTop)
                {
                    EmitExpression(lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get(), instructions, aliases);
                    EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, value, 3), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                }
                else
                {
                    EmitInstruction(new IRJmpIfEqInstruction(regId, value, 4), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                    EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
            }
            else
            {
                EmitExpression(lhs.get(), instructions, aliases);
                EmitExpression(rhs.get(), instructions, aliases);
                EmitInstruction(new IRSubInstruction(), instructions, expression, aliases);
                EmitInstruction(new IRJmpIfNotEqInstruction(Reg_StackTop, 0, 4), instructions, expression, aliases);
                EmitInstruction(new IRJmpIfSwSetInstruction(Switch_ArithmeticUnderflow, 3), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
            }
        }
        else if (op == OperatorType::NotEquals)
        {
            if (lhs->GetType() == ASTNodeType::NumberLiteral || rhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)(lhs->GetType() == ASTNodeType::NumberLiteral ? lhs.get() : rhs.get());
                auto value = numberLiteral->GetValue();

                auto other = lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get();
                auto regId = (int)Reg_StackTop;

                if (other->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)other;
                    regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (other->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)other;
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }

                if (regId == Reg_StackTop)
                {
                    EmitExpression(lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get(), instructions, aliases);
                    EmitInstruction(new IRJmpIfNotEqInstruction(Reg_StackTop, value, 3), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                }
                else
                {
                    EmitInstruction(new IRJmpIfNotEqInstruction(regId, value, 4), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                    EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
            }
            else
            {
                EmitExpression(lhs.get(), instructions, aliases);
                EmitExpression(rhs.get(), instructions, aliases);
                EmitInstruction(new IRSubInstruction(), instructions, expression, aliases);
                EmitInstruction(new IRJmpIfNotEqInstruction(Reg_StackTop, 0, 4), instructions, expression, aliases);
                EmitInstruction(new IRJmpIfSwSetInstruction(Switch_ArithmeticUnderflow, 3), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
            }
        }
        else if (op == OperatorType::LessThanOrEquals)
        {
            if (lhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)lhs.get();
                auto value = numberLiteral->GetValue();

                auto other = rhs.get();
                auto regId = (int)Reg_StackTop;

                if (other->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)other;
                    regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (other->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)other;
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }

                if (regId == Reg_StackTop)
                {
                    EmitExpression(rhs.get(), instructions, aliases);
                    EmitInstruction(new IRJmpIfGrtInstruction(Reg_StackTop, value, 3), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                }
                else
                {
                    EmitInstruction(new IRJmpIfGrtInstruction(regId, value, 4), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                    EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
            }
            else if (rhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)rhs.get();
                auto value = numberLiteral->GetValue();

                auto other = lhs.get();
                auto regId = (int)Reg_StackTop;

                if (other->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)other;
                    regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (other->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)other;
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }

                if (regId == Reg_StackTop)
                {
                    EmitExpression(lhs.get(), instructions, aliases);
                    EmitInstruction(new IRJmpIfLessOrEqualInstruction(Reg_StackTop, value, 3), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                }
                else
                {
                    EmitInstruction(new IRJmpIfLessOrEqualInstruction(regId, value, 4), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                    EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
            }
            else
            {
                EmitExpression(lhs.get(), instructions, aliases);
                EmitExpression(rhs.get(), instructions, aliases);
                EmitInstruction(new IRSubInstruction(), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                EmitInstruction(new IRJmpIfSwNotSetInstruction(Switch_ArithmeticUnderflow, 2), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
            }
        }
        else if (op == OperatorType::LessThan)
        {
            if (lhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)lhs.get();
                auto value = numberLiteral->GetValue();

                auto other = rhs.get();
                auto regId = (int)Reg_StackTop;

                if (other->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)other;
                    regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (other->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)other;
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }

                if (regId == Reg_StackTop)
                {
                    EmitExpression(rhs.get(), instructions, aliases);
                    EmitInstruction(new IRJmpIfGrtOrEqualInstruction(Reg_StackTop, value, 3), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                }
                else
                {
                    EmitInstruction(new IRJmpIfGrtOrEqualInstruction(regId, value, 4), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                    EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
            }
            else if (rhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)rhs.get();
                auto value = numberLiteral->GetValue();

                auto other = lhs.get();
                auto regId = (int)Reg_StackTop;

                if (other->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)other;
                    regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (other->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)other;
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }

                if (regId == Reg_StackTop)
                {
                    EmitExpression(lhs.get(), instructions, aliases);
                    EmitInstruction(new IRJmpIfLessInstruction(Reg_StackTop, value, 3), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                }
                else
                {
                    EmitInstruction(new IRJmpIfLessInstruction(regId, value, 4), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                    EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
            }
            else
            {
                EmitExpression(lhs.get(), instructions, aliases);
                EmitExpression(rhs.get(), instructions, aliases);
                EmitInstruction(new IRDecRegInstruction(Reg_StackTop + 1, 1), instructions, expression, aliases);
                EmitInstruction(new IRSubInstruction(), instructions, expression, aliases);
                EmitInstruction(new IRJmpIfSwNotSetInstruction(Switch_ArithmeticUnderflow, 3), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
            }
        }
        else if (op == OperatorType::GreaterThanOrEquals)
        {
            if (lhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)lhs.get();
                auto value = numberLiteral->GetValue();

                auto other = rhs.get();
                auto regId = (int)Reg_StackTop;

                if (other->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)other;
                    regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (other->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)other;
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }

                if (regId == Reg_StackTop)
                {
                    EmitExpression(rhs.get(), instructions, aliases);
                    EmitInstruction(new IRJmpIfLessInstruction(Reg_StackTop, value, 3), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                }
                else
                {
                    EmitInstruction(new IRJmpIfLessInstruction(regId, value, 4), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                    EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
            }
            else if (rhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)rhs.get();
                auto value = numberLiteral->GetValue();

                auto other = lhs.get();
                auto regId = (int)Reg_StackTop;

                if (other->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)other;
                    regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (other->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)other;
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }

                if (regId == Reg_StackTop)
                {
                    EmitExpression(lhs.get(), instructions, aliases);
                    EmitInstruction(new IRJmpIfGrtOrEqualInstruction(Reg_StackTop, value, 3), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                }
                else
                {
                    EmitInstruction(new IRJmpIfGrtOrEqualInstruction(regId, value, 4), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                    EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
            }
            else
            {
                EmitExpression(lhs.get(), instructions, aliases);
                EmitExpression(rhs.get(), instructions, aliases);
                EmitInstruction(new IRSubInstruction(), instructions, expression, aliases);
                EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, 4), instructions, expression, aliases);
                EmitInstruction(new IRJmpIfSwSetInstruction(Switch_ArithmeticUnderflow, 3), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
            }
        }
        else if (op == OperatorType::GreaterThan)
        {
            if (lhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)lhs.get();
                auto value = numberLiteral->GetValue();

                auto other = rhs.get();
                auto regId = (int)Reg_StackTop;

                if (other->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)other;
                    regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (other->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)other;
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }

                if (regId == Reg_StackTop)
                {
                    EmitExpression(rhs.get(), instructions, aliases);
                    EmitInstruction(new IRJmpIfLessOrEqualInstruction(Reg_StackTop, value, 3), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                }
                else
                {
                    EmitInstruction(new IRJmpIfLessOrEqualInstruction(regId, value, 4), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                    EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
            }
            else if (rhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)rhs.get();
                auto value = numberLiteral->GetValue();

                auto other = lhs.get();
                auto regId = (int)Reg_StackTop;

                if (other->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)other;
                    regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (other->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)other;
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }

                if (regId == Reg_StackTop)
                {
                    EmitExpression(lhs.get(), instructions, aliases);
                    EmitInstruction(new IRJmpIfGrtInstruction(Reg_StackTop, value, 3), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                    EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                }
                else
                {
                    EmitInstruction(new IRJmpIfGrtInstruction(regId, value, 4), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                    EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
            }
            else
            {
                EmitExpression(lhs.get(), instructions, aliases);
                EmitExpression(rhs.get(), instructions, aliases);
                EmitInstruction(new IRSubInstruction(), instructions, expression, aliases);
                EmitInstruction(new IRIncRegInstruction(Reg_StackTop + 1, 1), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                EmitInstruction(new IRJmpIfSwNotSetInstruction(Switch_ArithmeticUnderflow, 2), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
            }
        }
        else if (op == OperatorType::And)
        {
            if (lhs->GetType() == ASTNodeType::NumberLiteral || rhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)(lhs->GetType() == ASTNodeType::NumberLiteral ? lhs.get() : rhs.get());
                auto value = numberLiteral->GetValue();

                if (value == 0)
                {
                    EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                }
                else
                {
                    auto other = lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get();
                    auto regId = (int)Reg_StackTop;

                    if (other->GetType() == ASTNodeType::Identifier)
                    {
                        auto identifier = (ASTIdentifier*)other;
                        regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                    }
                    else if (other->GetType() == ASTNodeType::ArrayExpression)
                    {
                        auto arrayExpression = (ASTArrayExpression*)other;
                        auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                        regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                    }

                    if (regId == Reg_StackTop)
                    {
                        EmitExpression(lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get(), instructions, aliases);
                        EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, 3), instructions, expression, aliases);
                        EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                        EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                        EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    }
                    else
                    {
                        EmitInstruction(new IRJmpIfEqInstruction(regId, 0, 4), instructions, expression, aliases);
                        EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                        EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                        EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                        EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    }
                }
            }
            else
            {
                EmitExpression(lhs.get(), instructions, aliases);
                EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, 6), instructions, expression, aliases);
                EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                EmitExpression(rhs.get(), instructions, aliases);
                EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, 3), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
            }
        }
        else if (op == OperatorType::Or)
        {
            if (lhs->GetType() == ASTNodeType::NumberLiteral || rhs->GetType() == ASTNodeType::NumberLiteral)
            {
                auto numberLiteral = (ASTNumberLiteral*)(lhs->GetType() == ASTNodeType::NumberLiteral ? lhs.get() : rhs.get());
                auto value = numberLiteral->GetValue();

                if (value == 1)
                {
                    EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                }
                else
                {
                    auto other = lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get();
                    auto regId = (int)Reg_StackTop;

                    if (other->GetType() == ASTNodeType::Identifier)
                    {
                        auto identifier = (ASTIdentifier*)other;
                        regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                    }
                    else if (other->GetType() == ASTNodeType::ArrayExpression)
                    {
                        auto arrayExpression = (ASTArrayExpression*)other;
                        auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                        regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                    }

                    if (regId == Reg_StackTop)
                    {
                        EmitExpression(lhs->GetType() == ASTNodeType::NumberLiteral ? rhs.get() : lhs.get(), instructions, aliases);
                        EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, 3), instructions, expression, aliases);
                        EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
                        EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                        EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                    }
                    else
                    {
                        EmitInstruction(new IRJmpIfEqInstruction(regId, 0, 4), instructions, expression, aliases);
                        EmitInstruction(new IRPushInstruction(1, true), instructions, expression, aliases);
                        EmitInstruction(new IRJmpInstruction(3), instructions, expression, aliases);
                        EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                        EmitInstruction(new IRPushInstruction(0, true), instructions, expression, aliases);
                    }
                }
            }
            else
            {
                EmitExpression(lhs.get(), instructions, aliases);
                EmitInstruction(new IRJmpIfNotEqInstruction(Reg_StackTop, 0, 6), instructions, expression, aliases);
                EmitInstruction(new IRPopInstruction(), instructions, expression, aliases);
                EmitExpression(rhs.get(), instructions, aliases);
                EmitInstruction(new IRJmpIfNotEqInstruction(Reg_StackTop, 0, 3), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
                EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
                EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
            }
        }
        else
        {
            throw IRCompilerException("Unsupported operator", expression);
        }
    }

    void IRCompiler::EmitNotExpression(ASTUnaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases)
    {
        auto& lhs = expression->GetValue();

        auto op = expression->GetOperator();
        if (op == OperatorType::Not)
        {
            EmitExpression(lhs.get(), instructions, aliases);
            EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, 3), instructions, expression, aliases);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 0), instructions, expression, aliases);
            EmitInstruction(new IRJmpInstruction(2), instructions, expression, aliases);
            EmitInstruction(new IRSetRegInstruction(Reg_StackTop, 1), instructions, expression, aliases);
        }
        else
        {
            throw IRCompilerException("Unsupported operator", expression);
        }
    }

    void IRCompiler::EmitPostfixExpression(ASTUnaryExpression* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool pushToStack)
    {
        auto& lhs = expression->GetValue();
        auto op = expression->GetOperator();

        if (op == OperatorType::PostfixIncrement)
        {
            if (lhs->GetType() == ASTNodeType::Identifier)
            {
                auto identifier = (ASTIdentifier*)lhs.get();
                auto regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                if (regId == -1)
                {
                    throw IRCompilerException(SafePrintf("Invalid name \"%\"", identifier->GetName()), expression);
                }

                EmitInstruction(new IRIncRegInstruction(regId, 1), instructions, expression, aliases);

                if (pushToStack)
                {
                    EmitInstruction(new IRPushInstruction(regId), instructions, expression, aliases);
                }
            }
            else if (lhs->GetType() == ASTNodeType::ArrayExpression)
            {
                auto arrayExpression = (ASTArrayExpression*)lhs.get();
                auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                auto regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);

                EmitInstruction(new IRIncRegInstruction(regId, 1), instructions, expression, aliases);

                if (pushToStack)
                {
                    EmitInstruction(new IRPushInstruction(regId), instructions, expression, aliases);
                }
            }
            else
            {
                throw IRCompilerException("Invalid postfix increment operator usage", expression);
            }
        }
        else if (op == OperatorType::PostfixDecrement)
        {
            if (lhs->GetType() == ASTNodeType::Identifier)
            {
                auto identifier = (ASTIdentifier*)lhs.get();
                auto regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                if (regId == -1)
                {
                    throw IRCompilerException(SafePrintf("Invalid name \"%\"", identifier->GetName()), expression);
                }

                EmitInstruction(new IRDecRegInstruction(regId, 1), instructions, expression, aliases);

                if (pushToStack)
                {
                    EmitInstruction(new IRPushInstruction(regId), instructions, expression, aliases);
                }
            }
            else if (lhs->GetType() == ASTNodeType::ArrayExpression)
            {
                auto arrayExpression = (ASTArrayExpression*)lhs.get();
                auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                auto regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);

                EmitInstruction(new IRDecRegInstruction(regId, 1), instructions, expression, aliases);

                if (pushToStack)
                {
                    EmitInstruction(new IRPushInstruction(regId), instructions, expression, aliases);
                }
            }
            else
            {
                throw IRCompilerException("Invalid postfix decrement operator usage", expression);
            }
        }
        else
        {
            throw IRCompilerException("Unsupported operator", expression);
        }
    }

    void IRCompiler::EmitExpression(IASTNode* expression, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases)
    {
        if (expression->GetType() == ASTNodeType::NumberLiteral)
        {
            auto number = (ASTNumberLiteral*)expression;
            EmitInstruction(new IRPushInstruction(number->GetValue(), true), instructions, expression, aliases);
        }
        else if (expression->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)expression;
            auto regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
            EmitInstruction(new IRPushInstruction(regId), instructions, expression, aliases);
        }
        else if (expression->GetType() == ASTNodeType::BinaryExpression)
        {
            EmitBinaryExpression((ASTBinaryExpression*)expression, instructions, aliases);
        }
        else if (expression->GetType() == ASTNodeType::UnaryExpression)
        {
            auto unaryExpression = (ASTUnaryExpression*)expression;
            if (unaryExpression->GetOperator() == OperatorType::Not)
            {
                EmitNotExpression(unaryExpression, instructions, aliases);
            }
            else
            {
                EmitPostfixExpression(unaryExpression, instructions, aliases, true);
            }
        }
        else if (expression->GetType() == ASTNodeType::ArrayExpression)
        {
            auto arrayExpression = (ASTArrayExpression*)expression;
            auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
            auto regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
            EmitInstruction(new IRPushInstruction(regId), instructions, expression, aliases);
        }
        else if (expression->GetType() == ASTNodeType::FunctionCall)
        {
            EmitFunctionCall((ASTFunctionCall*)expression, instructions, aliases, false);
        }
        else
        {
            throw IRCompilerException("Unsupported expression value", expression);
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
                aliases.Allocate(name, variableDeclaration->GetArraySize(), statement.get());
                auto regId = aliases.GetAlias(name, 0, variableDeclaration);
                m_DebugStackFrames.back()->m_Variables.push_back(std::make_pair(regId, name));

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
                if (expression == nullptr)
                {
                    continue;
                }

                if (variableDeclaration->GetArraySize() > 1)
                {
                    throw IRCompilerException(SafePrintf("Array declarations cannot have initializers, see declaration of \"%\"",
                        variableDeclaration->GetName()), expression.get());
                }

                aliases.Allocate(variableDeclaration->GetName(), 1, statement.get());
                auto regId = RegisterNameToIndex(variableDeclaration->GetName(), 0, aliases, expression.get());
                if (expression->GetType() == ASTNodeType::NumberLiteral)
                {
                    auto number = (ASTNumberLiteral*)expression.get();
                    EmitInstruction(new IRSetRegInstruction(regId, number->GetValue()), instructions, expression.get(), aliases);
                }
                else
                {
                    EmitExpression(expression.get(), instructions, aliases);
                    EmitInstruction(new IRPopInstruction(regId), instructions, expression.get(), aliases);
                }
            }
            else if (statement->GetType() == ASTNodeType::AssignmentExpression)
            {
                auto expression = (ASTAssignmentExpression*)statement.get();

                auto lhsRegIndex = -1;

                auto& lhs = expression->GetLHSValue();
                if (lhs->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)lhs.get();

                    if (!IsRegisterName(identifier->GetName(), aliases, expression))
                    {
                        throw IRCompilerException("Assignment expression has invalid register on the left side", expression);
                    }

                    lhsRegIndex = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression);
                }
                else if (lhs->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)lhs.get();

                    auto& name = arrayExpression->GetIdentifier();

                    if (!IsRegisterName(name, aliases, expression))
                    {
                        throw IRCompilerException("Assignment expression has invalid register on the left side", expression);
                    }

                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    lhsRegIndex = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);
                }
                else
                {
                    throw IRCompilerException("Invalid type on left side of assignment expression, expected identifier or array expression", expression);
                }

                if (lhsRegIndex == -1)
                {
                    throw IRCompilerException("Invalid type on left side of assignment expression, expected identifier or array expression", expression);
                }

                auto& rhs = expression->GetRHSValue();

                if (rhs->GetType() == ASTNodeType::NumberLiteral)
                {
                    auto number = (ASTNumberLiteral*)rhs.get();
                    EmitInstruction(new IRSetRegInstruction(lhsRegIndex, number->GetValue()), instructions, rhs.get(), aliases);
                }
                else if (rhs->GetType() == ASTNodeType::Identifier)
                {
                    auto rhsRegister = (ASTIdentifier*)rhs.get();
                    auto rhsRegIndex = RegisterNameToIndex(rhsRegister->GetName(), 0, aliases, expression);

                    if (rhsRegIndex == -1)
                    {
                        throw IRCompilerException("Invalid value on right side of assignment expression", expression);
                    }

                    EmitInstruction(new IRCopyRegInstruction(lhsRegIndex, rhsRegIndex), instructions, rhs.get(), aliases);
                }
                else if (rhs->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)rhs.get();
                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());
                    auto rhsRegIndex = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression);

                    EmitInstruction(new IRCopyRegInstruction(lhsRegIndex, rhsRegIndex), instructions, rhs.get(), aliases);
                }
                else
                {
                    EmitExpression(rhs.get(), instructions, aliases);
                    EmitInstruction(new IRPopInstruction(lhsRegIndex), instructions, rhs.get(), aliases);
                }
            }
            else if (statement->GetType() == ASTNodeType::UnaryExpression)
            {
                auto unaryExpression = (ASTUnaryExpression*)statement.get();
                if (unaryExpression->GetOperator() == OperatorType::Not)
                {
                    EmitNotExpression(unaryExpression, instructions, aliases);
                }
                else
                {
                    EmitPostfixExpression(unaryExpression, instructions, aliases, false);
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
                    throw IRCompilerException("Invalid AST. If statement body must be a block statement", expression.get());
                }

                std::vector<std::unique_ptr<IIRInstruction>> bodyInstructions;
                EmitBlockStatement((ASTBlockStatement*)body.get(), bodyInstructions, aliases);

                if (bodyInstructions.size() == 0)
                {
                    throw IRCompilerException("Disallowed if statement with empty body", expression.get());
                }

                std::vector<std::unique_ptr<IIRInstruction>> elseBodyInstructions;
                auto& elseBody = ifStatement->GetElseBody();
                if (elseBody != nullptr)
                {
                    if (elseBody->GetType() != ASTNodeType::BlockStatement)
                    {
                        throw IRCompilerException("Invalid AST. If-else statement body must be a block statement", expression.get());
                    }

                    EmitBlockStatement((ASTBlockStatement*)elseBody.get(), elseBodyInstructions, aliases);

                    if (elseBodyInstructions.size() == 0)
                    {
                        throw IRCompilerException("Disallowed if-else statement with empty body", expression.get());
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
                        throw IRCompilerException("Disallowed if statement with empty body", expression.get());
                    }

                    auto regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression.get());
                    auto offset = 1;
                    if (elseBodyInstructions.size() > 0)
                    {
                        offset = 2;
                    }

                    EmitInstruction(new IRJmpIfEqInstruction(regId, 0, bodyInstructions.size() + offset), instructions, expression.get(), aliases);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    if (elseBodyInstructions.size() > 0)
                    {
                        EmitInstruction(new IRJmpInstruction(elseBodyInstructions.size() + 1), instructions, expression.get(), aliases);
                    }

                    for (auto& instruction : elseBodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }
                }
                else if (expression->GetType() == ASTNodeType::ArrayExpression)
                {
                    auto arrayExpression = (ASTArrayExpression*)expression.get();

                    if (bodyInstructions.size() == 0)
                    {
                        throw IRCompilerException("Disallowed if statement with empty body", expression.get());
                    }

                    auto arrayIndex = ParseArrayExpression(arrayExpression->GetIndex());

                    auto regId = RegisterNameToIndex(arrayExpression->GetIdentifier(), arrayIndex, aliases, expression.get());
                    auto offset = 1;
                    if (elseBodyInstructions.size() > 0)
                    {
                        offset = 2;
                    }

                    EmitInstruction(new IRJmpIfEqInstruction(regId, 0, bodyInstructions.size() + offset), instructions, arrayExpression, aliases);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    if (elseBodyInstructions.size() > 0)
                    {
                        EmitInstruction(new IRJmpInstruction(elseBodyInstructions.size() + 1), instructions, arrayExpression, aliases);
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

                    auto offset = 1;
                    if (elseBodyInstructions.size() > 0)
                    {
                        offset = 2;
                    }

                    EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, bodyInstructions.size() + offset), instructions, binaryExpression, aliases);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    if (elseBodyInstructions.size() > 0)
                    {
                        EmitInstruction(new IRJmpInstruction(elseBodyInstructions.size() + 1), instructions, binaryExpression, aliases);
                    }

                    for (auto& instruction : elseBodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }
                }
                else if (expression->GetType() == ASTNodeType::UnaryExpression)
                {
                    auto unaryExpression = (ASTUnaryExpression*)expression.get();

                    if (unaryExpression->GetOperator() == OperatorType::Not)
                    {
                        EmitNotExpression(unaryExpression, instructions, aliases);
                    }
                    else
                    {
                        EmitPostfixExpression(unaryExpression, instructions, aliases, true);
                    }

                    auto offset = 1;
                    if (elseBodyInstructions.size() > 0)
                    {
                        offset = 2;
                    }

                    EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, bodyInstructions.size() + offset), instructions, unaryExpression, aliases);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    if (elseBodyInstructions.size() > 0)
                    {
                        EmitInstruction(new IRJmpInstruction(elseBodyInstructions.size() + 1), instructions, unaryExpression, aliases);
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

                    EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, bodyInstructions.size() + offset), instructions, expression.get(), aliases);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    if (elseBodyInstructions.size() > 0)
                    {
                        EmitInstruction(new IRJmpInstruction(elseBodyInstructions.size() + 1), instructions, expression.get(), aliases);
                    }

                    for (auto& instruction : elseBodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }
                }
                else
                {
                    throw IRCompilerException("Unsupported type in expression", expression.get());
                }
            }
            else if (statement->GetType() == ASTNodeType::WhileStatement)
            {
                auto whileStatement = (ASTWhileStatement*)statement.get();
                auto expression = whileStatement->GetExpression();
                auto body = whileStatement->GetBody();

                if (body->GetType() != ASTNodeType::BlockStatement)
                {
                    throw IRCompilerException("Invalid AST. While statement body must be a block statement", expression.get());
                }

                if (expression->GetType() == ASTNodeType::NumberLiteral)
                {
                    auto numberLiteral = (ASTNumberLiteral*)expression.get();
                    if (numberLiteral->GetValue() > 0)
                    {
                        auto loopStart = instructions.size();
                        EmitBlockStatement((ASTBlockStatement*)body.get(), instructions, aliases);
                        EmitInstruction(new IRJmpInstruction(loopStart, true), instructions, expression.get(), aliases);
                    }
                }
                else if (expression->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)expression.get();

                    std::vector<std::unique_ptr<IIRInstruction>> bodyInstructions;
                    EmitBlockStatement((ASTBlockStatement*)body.get(), bodyInstructions, aliases);

                    if (bodyInstructions.size() == 0)
                    {
                        throw IRCompilerException("Disallowed while statement with empty body", expression.get());
                    }

                    auto loopStart = (int)instructions.size();
                    auto regId = RegisterNameToIndex(identifier->GetName(), 0, aliases, expression.get());
                    EmitInstruction(new IRJmpIfEqInstruction(regId, 0, bodyInstructions.size() + 2), instructions, expression.get(), aliases);

                    auto instructionCount = instructions.size();
                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    EmitInstruction(new IRJmpInstruction(loopStart - (int)instructions.size()), instructions, expression.get(), aliases);
                }
                else if (expression->GetType() == ASTNodeType::BinaryExpression)
                {
                    std::vector<std::unique_ptr<IIRInstruction>> bodyInstructions;
                    EmitBlockStatement((ASTBlockStatement*)body.get(), bodyInstructions, aliases);

                    if (bodyInstructions.size() == 0)
                    {
                        throw IRCompilerException("Disallowed while statement with empty body", expression.get());
                    }

                    auto loopStart = instructions.size();
                    auto binaryExpression = (ASTBinaryExpression*)expression.get();
                    EmitBinaryExpression(binaryExpression, instructions, aliases);
                    EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, bodyInstructions.size() + 2), instructions, expression.get(), aliases);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    EmitInstruction(new IRJmpInstruction(loopStart, true), instructions, expression.get(), aliases);
                }
                else if (expression->GetType() == ASTNodeType::UnaryExpression)
                {
                    std::vector<std::unique_ptr<IIRInstruction>> bodyInstructions;
                    EmitBlockStatement((ASTBlockStatement*)body.get(), bodyInstructions, aliases);

                    if (bodyInstructions.size() == 0)
                    {
                        throw IRCompilerException("Disallowed while statement with empty body", expression.get());
                    }

                    auto loopStart = instructions.size();
                    auto unaryExpression = (ASTUnaryExpression*)expression.get();
                    if (unaryExpression->GetOperator() == OperatorType::Not)
                    {
                        EmitNotExpression(unaryExpression, instructions, aliases);
                    }
                    else
                    {
                        EmitPostfixExpression(unaryExpression, instructions, aliases, true);
                    }

                    EmitInstruction(new IRJmpIfEqInstruction(Reg_StackTop, 0, bodyInstructions.size() + 2), instructions, expression.get(), aliases);

                    for (auto& instruction : bodyInstructions)
                    {
                        instructions.push_back(std::move(instruction));
                    }

                    EmitInstruction(new IRJmpInstruction(loopStart, true), instructions, expression.get(), aliases);
                }
                else
                {
                    throw IRCompilerException("Unsupported type in expression", expression.get());
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
                        EmitInstruction(new IRPushInstruction(number->GetValue(), true), instructions, expression.get(), aliases);
                    }
                    else
                    {
                        EmitExpression(returnStatement->GetExpression().get(), instructions, aliases);
                    }
                }

                EmitInstruction(new IRJmpInstruction(JMP_TO_END_OFFSET_CONSTANT), instructions, statement.get(), aliases);
            }
            else
            {
                throw IRCompilerException("Unsupported statement type in function body", statement.get());
            }

            statementIndex++;
        }

        for (auto& name : localVariables)
        {
            aliases.Deallocate(name, blockStatement);
        }

        return startIndex;
    }

    unsigned int IRCompiler::EmitFunction(ASTFunctionDeclaration* fn, std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases)
    {
        auto frame = std::make_shared<StackFrame>();
        frame->m_ASTNode = fn;
        frame->m_FunctionName = fn->GetName();

        auto body = fn->GetChild(0);
        if (body->GetType() != ASTNodeType::BlockStatement)
        {
            throw IRCompilerException("Invalid AST. Function body is not a block statement", fn);
        }

        auto blockStatement = (ASTBlockStatement*)body.get();

        auto startIndex = instructions.size();

        auto argsCount = fn->GetArgumentCount();

        auto& args = fn->GetArguments();
        for (auto i = 0u; i < argsCount; i++)
        {
            auto& argName = args[i];
            aliases.Allocate(argName, 1, fn);
            auto regId = aliases.GetAlias(argName, 0, fn);

            frame->m_Variables.push_back(std::make_pair(regId, argName));

            EmitInstruction(new IRPopInstruction(regId), instructions, fn, aliases);
        }

        m_DebugStackFrames.push_back(frame);

        auto instructionsStart = instructions.size();
        EmitBlockStatement(blockStatement, instructions, aliases);

        for (auto i = instructionsStart; i < instructions.size(); i++)
        {
            auto& instruction = instructions[i];
            if (instruction->GetType() == IRInstructionType::Jmp)
            {
                auto jmp = (IRJmpInstruction*)instruction.get();
                if (jmp->GetOffset() == JMP_TO_END_OFFSET_CONSTANT)
                {
                    jmp->SetOffset(instructions.size() - i);
                }
            }
        }

        for (auto i = 0u; i < argsCount; i++)
        {
            auto& argName = args[i];
            aliases.Deallocate(argName, fn);
        }

        if (instructions.size() == 0)
        {
            throw IRCompilerException(SafePrintf("Function \"%\" has an empty body", fn->GetName()), fn);
        }

        if (fn->GetName() == "main")
        {
            if (instructions.back()->GetType() != IRInstructionType::Jmp)
            {
                EmitInstruction(new IRJmpInstruction(startIndex, true), instructions, fn, aliases);
            }
        }
        else if (instructions.back()->GetType() != IRInstructionType::Push)
        {
            //EmitInstruction(new IRPushInstruction(0, true), instructions);
        }

        m_DebugStackFrames.pop_back();

        return startIndex;
    }

    bool IRCompiler::IsRegisterName(const std::string& name, RegisterAliases& aliases, IASTNode* node) const
    {
        auto regId = aliases.GetAlias(name, 0, node);
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

    int IRCompiler::RegisterNameToIndex(const std::string& name, unsigned int arrayIndex, RegisterAliases& aliases, IASTNode* node) const
    {
        auto regId = aliases.GetAlias(name, arrayIndex, node);
        if (regId == -1)
        {
            std::string reg(name.c_str() + 1, name.length() - 1);
            auto index = std::atoi(reg.c_str());
            if (index == 0)
            {
                throw IRCompilerException(SafePrintf("Invalid register \"%\"", name), node);
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

    unsigned int IRCompiler::ParseArrayExpression(const std::shared_ptr<IASTNode>& indexExpression)
    {
        if (indexExpression->GetType() == ASTNodeType::NumberLiteral)
        {
            auto index = (ASTNumberLiteral*)indexExpression.get();
            return index->GetValue();
        }
        else if (indexExpression->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)indexExpression.get();
            auto playerId = PlayerNameToId(identifier->GetName());
            if (playerId == -1)
            {
                throw IRCompilerException(SafePrintf("Invalid value \"%\" used in array expression", identifier->GetName()), indexExpression.get());
            }

            return playerId;
        }
        else
        {
            throw IRCompilerException("Invalid type used in array expression", indexExpression.get());
        }
    }

    uint8_t IRCompiler::ParsePlayerIdArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in \"%\", expected player name", argIndex, fnName), node.get());
        }

        auto identifier = (ASTIdentifier*)node.get();
        auto& name = identifier->GetName();
        auto playerId = PlayerNameToId(name);
        if (playerId == -1)
        {
            throw IRCompilerException(SafePrintf("Invalid player name \"%\" given as argument % in \"%\"", name, argIndex, fnName), node.get());
        }

        return playerId;
    }

    ConditionComparison IRCompiler::ParseComparisonArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected comparison type (AtLeast, Exactly, AtMost)", argIndex, fnName), node.get());
        }

        auto identifier = (ASTIdentifier*)node.get();
        auto& name = identifier->GetName();

        ConditionComparison comparison;
        if (name == "AtLeast" || name == "GreaterOrEquals")
        {
            comparison = ConditionComparison::AtLeast;
        }
        else if (name == "AtMost" || name == "LessOrEquals")
        {
            comparison = ConditionComparison::AtMost;
        }
        else if (name == "Exactly" || name == "Equals")
        {
            comparison = ConditionComparison::Exactly;
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected comparison type (AtLeast, Exactly, AtMost)", argIndex, fnName), node.get());
        }

        return comparison;
    }

    int IRCompiler::ParseQuantityArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto string = (ASTStringLiteral*)node.get();
            if (string->GetValue() == "All")
            {
                return 0;
            }

            throw IRCompilerException(SafePrintf("Invalid argument value for argument % in call to \"%\", expected quantity", argIndex, fnName), node.get());
        }
        else if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            if (identifier->GetName() == "All")
            {
                return 0;
            }

            throw IRCompilerException(SafePrintf("Invalid argument value for argument % in call to \"%\", expected quantity", argIndex, fnName), node.get());
        }

        if (node->GetType() != ASTNodeType::NumberLiteral)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected quantity", argIndex, fnName), node.get());
        }

        auto numberLiteral = (ASTNumberLiteral*)node.get();
        return numberLiteral->GetValue();
    }

    uint8_t IRCompiler::ParseUnitTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected unit type", argIndex, fnName), node.get());
        }

        auto identifier = (ASTIdentifier*)node.get();
        auto& name = identifier->GetName();
        auto unitId = UnitNameToId(name);

        if (unitId == -1)
        {
            throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed for argument % in call to \"%\"", name, argIndex, fnName), node.get());
        }

        return (uint8_t)unitId;
    }

    uint32_t IRCompiler::ParseAIScriptArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {

        if (node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected AI script type", argIndex, fnName), node.get());
        }

        auto identifier = (ASTIdentifier*)node.get();
        auto& name = identifier->GetName();

        for (auto i = 0; i < sizeof(AIScriptTypeNames); i++)
        {
            if (AIScriptTypeNames[i] == name)
            {
                auto& scriptName = AIScriptNames[i];
                uint32_t script;
                memcpy(&script, scriptName.data(), 4);
                return script;
            }
        }

        throw IRCompilerException(SafePrintf("Invalid argument value \"%\" for argument % in call to \"%\", expected AI script type", name, argIndex, fnName), node.get());
    }

    std::string IRCompiler::ParseLocationArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected location name", argIndex, fnName), node.get());
        }

        std::string locationName;

        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto stringLiteral = (ASTStringLiteral*)node.get();
            return stringLiteral->GetValue();
        }
        else if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            return identifier->GetName();
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected location name", argIndex, fnName), node.get());
        }
    }

    CHK::ResourceType IRCompiler::ParseResourceTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Minerals or Gas", argIndex, fnName), node.get());
        }

        std::string name;

        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto stringLiteral = (ASTStringLiteral*)node.get();
            name = stringLiteral->GetValue();
        }
        else if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            name = identifier->GetName();
        }

        if (name == "Minerals" || name == "Ore")
        {
            return CHK::ResourceType::Ore;
        }
        else if (name == "Gas" || name == "Vespene")
        {
            return CHK::ResourceType::Gas;
        }
        else if (name == "OreAndGas" || name == "Both" || name == "All")
        {
            return CHK::ResourceType::OreAndGas;
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid argument value \"%\" for argument % in call to \"%\", expected Minerals or Gas", name, argIndex, fnName), node.get());
        }
    }

    CHK::ScoreType IRCompiler::ParseScoreTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected score type", argIndex, fnName), node.get());
        }

        std::string name;

        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto stringLiteral = (ASTStringLiteral*)node.get();
            name = stringLiteral->GetValue();
        }
        else if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            name = identifier->GetName();
        }

        if (name == "Total")
        {
            return CHK::ScoreType::Total;
        }
        else if (name == "Units")
        {
            return CHK::ScoreType::Units;
        }
        else if (name == "Buildings")
        {
            return CHK::ScoreType::Buildings;
        }
        else if (name == "UnitsAndBuildings")
        {
            return CHK::ScoreType::UnitsAndBuildings;
        }
        else if (name == "Kills")
        {
            return CHK::ScoreType::Kills;
        }
        else if (name == "Razings")
        {
            return CHK::ScoreType::Razings;
        }
        else if (name == "KillsAndRazings")
        {
            return CHK::ScoreType::KillsAndRazings;
        }
        else if (name == "Custom")
        {
            return CHK::ScoreType::Custom;
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid argument value \"%\" for argument % in call to \"%\", expected score type", name, argIndex, fnName), node.get());
        }
    }

    LeaderboardType IRCompiler::ParseLeaderboardType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected leaderboard type", argIndex, fnName), node.get());
        }

        std::string name;

        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto stringLiteral = (ASTStringLiteral*)node.get();
            name = stringLiteral->GetValue();
        }
        else if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            name = identifier->GetName();
        }

        if (name == "ControlAtLocation")
        {
            return LeaderboardType::ControlAtLocation;
        }
        else if (name == "Control")
        {
            return LeaderboardType::Control;
        }
        else if (name == "Greed")
        {
            return LeaderboardType::Greed;
        }
        else if (name == "Kills")
        {
            return LeaderboardType::Kills;
        }
        else if (name == "Points")
        {
            return LeaderboardType::Points;
        }
        else if (name == "Resources")
        {
            return LeaderboardType::Resources;
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid argument value \"%\" for argument % in call to \"%\", expected leaderboard type", name, argIndex, fnName), node.get());
        }
    }

    AllianceStatus IRCompiler::ParseAllianceStatusArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Ally, Enemy or AlliedVictory", argIndex, fnName), node.get());
        }

        std::string name;

        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto stringLiteral = (ASTStringLiteral*)node.get();
            name = stringLiteral->GetValue();
        }
        else if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            name = identifier->GetName();
        }

        if (name == "Ally")
        {
            return AllianceStatus::Ally;
        }
        else if (name == "Enemy")
        {
            return AllianceStatus::Enemy;
        }
        else if (name == "AlliedVictory")
        {
            return AllianceStatus::AlliedVictory;
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid argument value \"%\" for argument % in call to \"%\", expected Ally, Enemy or AlliedVictory", name, argIndex, fnName), node.get());
        }
    }

    EndGameType IRCompiler::ParseEndGameCondition(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Victory, Defeat or Draw", argIndex, fnName), node.get());
        }

        std::string name;

        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto stringLiteral = (ASTStringLiteral*)node.get();
            name = stringLiteral->GetValue();
        }
        else if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            name = identifier->GetName();
        }

        if (name == "Victory")
        {
            return EndGameType::Victory;
        }
        else if (name == "Defeat")
        {
            return EndGameType::Defeat;
        }
        else if (name == "Draw")
        {
            return EndGameType::Draw;
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid argument value for argument % in call to \"%\", expected Victory, Defeat or Draw", argIndex, fnName), node.get());
        }
    }

    CHK::TriggerActionState IRCompiler::ParseOrderType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Move, Attack or Patrol", argIndex, fnName), node.get());
        }

        std::string name;

        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto stringLiteral = (ASTStringLiteral*)node.get();
            name = stringLiteral->GetValue();
        }
        else if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            name = identifier->GetName();
        }

        if (name == "Move")
        {
            return CHK::TriggerActionState::Move;
        }
        else if (name == "Attack")
        {
            return CHK::TriggerActionState::Attack;
        }
        else if (name == "Patrol")
        {
            return CHK::TriggerActionState::Patrol;
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid argument value for argument % in call to \"%\", expected Move, Attack or Patrol", argIndex, fnName), node.get());
        }
    }

    CHK::TriggerActionState IRCompiler::ParseToggleState(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Enable, Disable or Toggle", argIndex, fnName), node.get());
        }

        std::string name;

        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto stringLiteral = (ASTStringLiteral*)node.get();
            name = stringLiteral->GetValue();
        }
        else if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            name = identifier->GetName();
        }

        if (name == "Enable")
        {
            return CHK::TriggerActionState::SetSwitch;
        }
        else if (name == "Disable")
        {
            return CHK::TriggerActionState::ClearSwitch;
        }
        else if (name == "Toggle")
        {
            return CHK::TriggerActionState::ToggleSwitch;
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid argument value for argument % in call to \"%\", expected Enable, Disable or Toggle", argIndex, fnName), node.get());
        }
    }

    ModifyType IRCompiler::ParseModifyType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Move, Attack or Patrol", argIndex, fnName), node.get());
        }

        std::string name;

        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto stringLiteral = (ASTStringLiteral*)node.get();
            name = stringLiteral->GetValue();
        }
        else if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            name = identifier->GetName();
        }

        if (name == "Health")
        {
            return ModifyType::HitPoints;
        }
        else if (name == "Energy")
        {
            return ModifyType::Energy;
        }
        else if (name == "Shields")
        {
            return ModifyType::ShieldPoints;
        }
        else if (name == "Hangar")
        {
            return ModifyType::HangarCount;
        }
        else
        {
            throw IRCompilerException(SafePrintf("Invalid argument value for argument % in call to \"%\", expected Health, Energy, Shields or Hangar", argIndex, fnName), node.get());
        }
    }

    UnitPropType IRCompiler::ParseUnitPropType(const std::string& propName, IASTNode* node)
    {
        if (propName == "HitPoints" || propName == "Health")
        {
            return UnitPropType::HitPoints;
        }
        else if (propName == "ShieldPoints" || propName == "Shields")
        {
            return UnitPropType::ShieldPoints;
        }
        else if (propName == "Energy")
        {
            return UnitPropType::Energy;
        }
        else if (propName == "ResourceAmount")
        {
            return UnitPropType::ResourceAmount;
        }
        else if (propName == "HangarCount")
        {
            return UnitPropType::HangarCount;
        }
        else if (propName == "Cloaked")
        {
            return UnitPropType::Cloaked;
        }
        else if (propName == "Burrowed")
        {
            return UnitPropType::Burrowed;
        }
        else if (propName == "InTransit")
        {
            return UnitPropType::InTransit;
        }
        else if (propName == "Hallucinated")
        {
            return UnitPropType::Hallucinated;
        }
        else if (propName == "Invincible")
        {
            return UnitPropType::Invincible;
        }

        throw IRCompilerException(SafePrintf("Invalid unit property type \"%\"", propName), node);
    }

    int IRCompiler::ParseQuantityExpression(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex,
        std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool& isLiteral)
    {
        auto regId = 0;

        if (node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            if (identifier->GetName() == "All")
            {
                isLiteral = true;
                regId = 0;
            }
            else
            {
                EmitExpression(node.get(), instructions, aliases);
                isLiteral = false;
                regId = Reg_StackTop;
            }
        }
        else if (node->GetType() == ASTNodeType::NumberLiteral)
        {
            auto unitQuantity = (ASTNumberLiteral*)node.get();
            regId = unitQuantity->GetValue();
            isLiteral = true;
        }
        else
        {
            EmitExpression(node.get(), instructions, aliases);
            isLiteral = false;
            regId = Reg_StackTop;
        }

        return regId;
    }

}
