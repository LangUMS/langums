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
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto comparison = ParseComparisonArgument(condition->GetArgument(1), name, 1);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(2), name, 2);
                        auto unitId = ParseUnitTypeArgument(condition->GetArgument(3), name, 3);
                        auto locationName = ParseLocationArgument(condition->GetArgument(4), name, 4);

                        EmitInstruction(new IRBringCondInstruction(playerId, unitId, locationName, comparison, quantity), m_Instructions);
                    }
                    else if (name == "commands" || name == "killed" || name == "deaths")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto comparison = ParseComparisonArgument(condition->GetArgument(1), name, 1);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(2), name, 2);
                        auto unitId = ParseUnitTypeArgument(condition->GetArgument(3), name, 3);

                        if (name == "commands")
                        {
                            EmitInstruction(new IRCmdCondInstruction(playerId, unitId, comparison, quantity), m_Instructions);
                        }
                        else if (name == "killed")
                        {
                            EmitInstruction(new IRKillCondInstruction(playerId, unitId, comparison, quantity), m_Instructions);
                        }
                        else if (name == "deaths")
                        {
                            EmitInstruction(new IRDeathCondInstruction(playerId, unitId, comparison, quantity), m_Instructions);
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
                            EmitInstruction(new IRCmdLeastCondInstruction(playerId, unitId, locationName), m_Instructions);
                        }
                        else if (name == "commands_most")
                        {
                            EmitInstruction(new IRCmdMostCondInstruction(playerId, unitId, locationName), m_Instructions);
                        }
                    }
                    else if (name == "killed_least" || name == "killed_most")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto unitId = ParseUnitTypeArgument(condition->GetArgument(1), name, 1);

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
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto comparison = ParseComparisonArgument(condition->GetArgument(1), name, 1);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(2), name, 2);
                        auto resType = ParseResourceTypeArgument(condition->GetArgument(3), name, 3);

                        EmitInstruction(new IRAccumCondInstruction(playerId, resType, comparison, quantity), m_Instructions);
                    }
                    else if (name == "least_resources" || name == "most_resources")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto resType = ParseResourceTypeArgument(condition->GetArgument(1), name, 1);

                        if (name == "least_resources")
                        {
                            EmitInstruction(new IRLeastResCondInstruction(playerId, resType), m_Instructions);
                        }
                        else if (name == "most_resources")
                        {
                            EmitInstruction(new IRMostResCondInstruction(playerId, resType), m_Instructions);
                        }
                    }
                    else if (name == "elapsed_time")
                    {
                        auto comparison = ParseComparisonArgument(condition->GetArgument(0), name, 0);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(1), name, 1);

                        EmitInstruction(new IRTimeCondInstruction(comparison, quantity), m_Instructions);
                    }
                    else if (name == "countdown")
                    {
                        auto comparison = ParseComparisonArgument(condition->GetArgument(0), name, 0);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(1), name, 1);

                        EmitInstruction(new IRCountdownCondInstruction(comparison, quantity), m_Instructions);
                    }
                    else if (name == "opponents")
                    {
                        auto playerId = ParsePlayerIdArgument(condition->GetArgument(0), name, 0);
                        auto comparison = ParseComparisonArgument(condition->GetArgument(1), name, 1);
                        auto quantity = ParseQuantityArgument(condition->GetArgument(2), name, 2);

                        EmitInstruction(new IROpponentsCondInstruction(playerId, comparison, quantity), m_Instructions);
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

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto endCondition = ParseEndGameCondition(fnCall->GetArgument(1), fnName, 1);

            EmitInstruction(new IREndGameInstruction(playerId + 1, endCondition), instructions);
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

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto resType = ParseResourceTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRSetResourceInstruction(playerId, resType, regId, isLiteral), instructions);
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

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto resType = ParseResourceTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRIncResourceInstruction(playerId, resType, regId, isLiteral), instructions);
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

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto resType = ParseResourceTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRDecResourceInstruction(playerId, resType, regId, isLiteral), instructions);
        }
        else if (fnName == "set_score")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_score() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("set_score() takes exactly three arguments");
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto scoreType = ParseScoreTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRSetScoreInstruction(playerId, scoreType, regId, isLiteral), instructions);
        }
        else if (fnName == "add_score")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("add_score() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("add_score() takes exactly three arguments");
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto scoreType = ParseScoreTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRIncScoreInstruction(playerId, scoreType, regId, isLiteral), instructions);
        }
        else if (fnName == "subtract_score")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("subtract_score() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("subtract_score() takes exactly three arguments");
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto scoreType = ParseScoreTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRDecScoreInstruction(playerId, scoreType, regId, isLiteral), instructions);
        }
        else if (fnName == "set_score")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_score() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("set_score() takes exactly three arguments");
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto scoreType = ParseScoreTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRSetScoreInstruction(playerId, scoreType, regId, isLiteral), instructions);
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

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(0), fnName, 0, instructions, aliases, isLiteral);

            EmitInstruction(new IRSetCountdownInstruction(regId, isLiteral), instructions);
        }
        else if (fnName == "pause_countdown()")
        {
            EmitInstruction(new IRPauseCountdownInstruction(false), instructions);
        }
        else if (fnName == "unpause_countdown()")
        {
            EmitInstruction(new IRPauseCountdownInstruction(true), instructions);
        }
        else if (fnName == "mute_unit_speech()")
        {
            EmitInstruction(new IRMuteUnitSpeechInstruction(false), instructions);
        }
        else if (fnName == "unmute_unit_speech()")
        {
            EmitInstruction(new IRMuteUnitSpeechInstruction(true), instructions);
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

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRSetDeathsInstruction(playerId, unitId, regId, isLiteral), instructions);
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

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRIncDeathsInstruction(playerId, unitId, regId, isLiteral), instructions);
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

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            EmitInstruction(new IRDecDeathsInstruction(playerId, unitId, regId, isLiteral), instructions);
        }
        else if (fnName == "talking_portrait")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("talking_portrait() called without arguments");
            }

            if (fnCall->GetChildCount() != 3)
            {
                throw IRCompilerException("talking_portrait() takes exactly three arguments");
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);
            auto time = ParseQuantityArgument(fnCall->GetArgument(2), fnName, 2);
            EmitInstruction(new IRTalkInstruction(playerId, unitId, time * 1000), instructions);
        }
        else if (fnName == "set_doodad")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_doodad() called without arguments");
            }

            if (fnCall->GetChildCount() != 4)
            {
                throw IRCompilerException("set_doodad() takes exactly four arguments");
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);
            auto state = ParseToggleState(fnCall->GetArgument(2), fnName, 2);
            auto locationName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);

            EmitInstruction(new IRSetDoodadInstruction(playerId, unitId, locationName, state), instructions);
        }
        else if (fnName == "set_invincibility")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_invincibility() called without arguments");
            }

            if (fnCall->GetChildCount() != 4)
            {
                throw IRCompilerException("set_invincibility() takes exactly four arguments");
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(1), fnName, 1);
            auto state = ParseToggleState(fnCall->GetArgument(2), fnName, 2);
            auto locationName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);

            EmitInstruction(new IRSetInvincibleInstruction(playerId, unitId, locationName, state), instructions);
        }
        else if (fnName == "run_ai_script")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_invincibility() called without arguments");
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto scriptName = ParseAIScriptArgument(fnCall->GetArgument(1), fnName, 1);

            std::string locationName;
            if (fnCall->GetChildCount() > 1)
            {
                locationName = ParseLocationArgument(fnCall->GetArgument(2), fnName, 2);
            }

            EmitInstruction(new IRAIScriptInstruction(playerId, scriptName, locationName), instructions);
        }
        else if (fnName == "set_alliance")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("set_alliance() called without arguments");
            }

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto targetPlayerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            auto status = ParseAllianceStatusArgument(fnCall->GetArgument(2), fnName, 2);

            EmitInstruction(new IRSetAllyInstruction(playerId, targetPlayerId, status), instructions);
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

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto locName = ParseLocationArgument(fnCall->GetArgument(1), fnName, 1);

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

            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(0), fnName, 0);
            auto locName = ParseLocationArgument(fnCall->GetArgument(1), fnName, 1);

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
            auto playerId = -1;

            if (fnCall->GetChildCount() >= 2)
            {
                playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            }

            EmitInstruction(new IRDisplayMsgInstruction(msg->GetValue(), playerId), instructions);
        }
        else if (fnName == "sleep")
        {
            if (!fnCall->HasChildren())
            {
                throw IRCompilerException("sleep() called without arguments");
            }

            auto quantity = ParseQuantityArgument(fnCall->GetArgument(0), fnName, 0);
            EmitInstruction(new IRWaitInstruction(quantity), instructions);
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

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);
            
            std::string locName;
            if (fnCall->GetChildCount() == 4)
            {
                locName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);
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

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            auto srcLocName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);
            auto dstLocName = ParseLocationArgument(fnCall->GetArgument(4), fnName, 4);

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

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            auto order = ParseOrderType(fnCall->GetArgument(2), fnName, 2);
            auto srcLocName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);
            auto dstLocName = ParseLocationArgument(fnCall->GetArgument(4), fnName, 4);

            EmitInstruction(new IROrderInstruction(playerId, unitId, order, srcLocName, dstLocName), instructions);
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

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(2), fnName, 2, instructions, aliases, isLiteral);

            auto arg3 = fnCall->GetArgument(3);
            if (arg3->GetType() != ASTNodeType::Identifier)
            {
                throw IRCompilerException(SafePrintf("Something other than a string literal as third argument to %(), expected Health, Energy, Shields or Hangar", fnName));
            }

            auto modifyType = ParseModifyType(fnCall->GetArgument(3), fnName, 3);
            auto amount = ParseQuantityArgument(fnCall->GetArgument(4), fnName, 4);
            auto locName = ParseLocationArgument(fnCall->GetArgument(5), fnName, 5);

            EmitInstruction(new IRModifyInstruction(playerId, unitId, regId, isLiteral, amount, modifyType, locName), instructions);
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

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto srcPlayerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            auto dstPlayerId = ParsePlayerIdArgument(fnCall->GetArgument(2), fnName, 2);

            bool isLiteral = false;
            auto regId = ParseQuantityExpression(fnCall->GetArgument(3), fnName, 3, instructions, aliases, isLiteral);

            auto locName = ParseLocationArgument(fnCall->GetArgument(4), fnName, 4);

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

            auto unitId = ParseUnitTypeArgument(fnCall->GetArgument(0), fnName, 0);
            auto playerId = ParsePlayerIdArgument(fnCall->GetArgument(1), fnName, 1);
            auto srcLocName = ParseLocationArgument(fnCall->GetArgument(2), fnName, 2);
            auto dstLocName = ParseLocationArgument(fnCall->GetArgument(3), fnName, 3);

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

    uint8_t IRCompiler::ParsePlayerIdArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in \"%\", expected player name", argIndex, fnName));
        }

        auto identifier = (ASTIdentifier*)node.get();
        auto& name = identifier->GetName();
        auto playerId = PlayerNameToId(name);
        if (playerId == -1)
        {
            throw new IRCompilerException(SafePrintf("Invalid player name \"%\" given as argument % in \"%\"", name, argIndex, fnName));
        }

        return playerId;
    }

    ConditionComparison IRCompiler::ParseComparisonArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected comparison type (AtLeast, Exactly, AtMost)", argIndex, fnName));
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
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected comparison type (AtLeast, Exactly, AtMost)", argIndex, fnName));
        }

        return comparison;
    }

    int IRCompiler::ParseQuantityArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::NumberLiteral)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected quantity", argIndex, fnName));
        }

        auto numberLiteral = (ASTNumberLiteral*)node.get();
        return numberLiteral->GetValue();
    }

    uint8_t IRCompiler::ParseUnitTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected unit type", argIndex, fnName));
        }

        auto identifier = (ASTIdentifier*)node.get();
        auto& name = identifier->GetName();
        auto unitId = UnitNameToId(name);

        if (unitId == -1)
        {
            throw IRCompilerException(SafePrintf("Invalid unit name \"%\" passed for argument % in call to \"%\"", name, argIndex, fnName));
        }

        return (uint8_t)unitId;
    }

    uint32_t IRCompiler::ParseAIScriptArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {

        if (node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected AI script type", argIndex, fnName));
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

        throw new IRCompilerException(SafePrintf("Invalid argument value \"%\" for argument % in call to \"%\", expected AI script type", name, argIndex, fnName));
    }

    std::string IRCompiler::ParseLocationArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected location name", argIndex, fnName));
        }

        std::string locationName;

        if (node->GetType() == ASTNodeType::StringLiteral)
        {
            auto stringLiteral = (ASTStringLiteral*)node.get();
            return stringLiteral->GetValue();
        }
        else if(node->GetType() == ASTNodeType::Identifier)
        {
            auto identifier = (ASTIdentifier*)node.get();
            return identifier->GetName();
        }
        else
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected location name", argIndex, fnName));
        }
    }

    CHK::ResourceType IRCompiler::ParseResourceTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected location name", argIndex, fnName));
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
            throw new IRCompilerException(SafePrintf("Invalid argument value \"%\" for argument % in call to \"%\", expected Minerals or Gas", name, argIndex, fnName));
        }
    }

    CHK::ScoreType IRCompiler::ParseScoreTypeArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected score type", argIndex, fnName));
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
            throw new IRCompilerException(SafePrintf("Invalid argument value \"%\" for argument % in call to \"%\", expected score type", name, argIndex, fnName));
        }
    }

    AllianceStatus IRCompiler::ParseAllianceStatusArgument(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Ally, Enemy or AlliedVictory", argIndex, fnName));
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
            throw new IRCompilerException(SafePrintf("Invalid argument value \"%\" for argument % in call to \"%\", expected Ally, Enemy or AlliedVictory", name, argIndex, fnName));
        }
    }

    EndGameType IRCompiler::ParseEndGameCondition(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Victory, Defeat or Draw", argIndex, fnName));
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
            throw new IRCompilerException(SafePrintf("Invalid argument value for argument % in call to \"%\", expected Victory, Defeat or Draw", argIndex, fnName));
        }
    }

    CHK::TriggerActionState IRCompiler::ParseOrderType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Move, Attack or Patrol", argIndex, fnName));
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
            throw new IRCompilerException(SafePrintf("Invalid argument value for argument % in call to \"%\", expected Move, Attack or Patrol", argIndex, fnName));
        }
    }

    CHK::TriggerActionState IRCompiler::ParseToggleState(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Enable, Disable or Toggle", argIndex, fnName));
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
            throw new IRCompilerException(SafePrintf("Invalid argument value for argument % in call to \"%\", expected Enable, Disable or Toggle", argIndex, fnName));
        }
    }

    ModifyType IRCompiler::ParseModifyType(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex)
    {
        if (node->GetType() != ASTNodeType::StringLiteral && node->GetType() != ASTNodeType::Identifier)
        {
            throw new IRCompilerException(SafePrintf("Invalid argument type for argument % in call to \"%\", expected Move, Attack or Patrol", argIndex, fnName));
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
            throw new IRCompilerException(SafePrintf("Invalid argument value for argument % in call to \"%\", expected Health, Energy, Shields or Hangar", argIndex, fnName));
        }
    }

    int IRCompiler::ParseQuantityExpression(const std::shared_ptr<IASTNode>& node, const std::string& fnName, unsigned int argIndex,
        std::vector<std::unique_ptr<IIRInstruction>>& instructions, RegisterAliases& aliases, bool& isLiteral)
    {
        auto regId = 0;
        if (node->GetType() == ASTNodeType::NumberLiteral)
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
