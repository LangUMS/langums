#include <set>

#include "template_instantiator.h"
#include "parser.h"

namespace Langums
{

    void TemplateInstantiator::Process(const std::unique_ptr<IASTNode>& unit)
    {
        m_Unit = unit.get();

        if (unit->GetType() != ASTNodeType::Unit)
        {
            throw TemplateInstantiatorException("Internal error. Invalid AST node type, expected Unit", unit.get());
        }

        auto statements = m_Unit->GetChildren();

        for (auto& statement : statements)
        {
            if (statement->GetType() == ASTNodeType::TemplateFunction)
            {
                auto templateFn = (ASTTemplateFunction*)statement.get();
                auto& fnName = templateFn->GetName();

                if (m_TemplateFunctions.find(fnName) != m_TemplateFunctions.end())
                {
                    throw TemplateInstantiatorException(SafePrintf("Second definition of templated function \"%\"", fnName), statement.get());
                }

                m_TemplateFunctions.insert(std::make_pair(fnName, templateFn));
            }
        }

        InstantiateRepeatTemplates(unit.get());
        InstantiateTemplateFunctions(unit.get());
    }

    void TemplateInstantiator::InstantiateRepeatTemplates(IASTNode* node)
    {
        auto children = node->GetChildren();
        node->RemoveChildren();

        std::vector<std::shared_ptr<IASTNode>> newChildren;

        for (auto& child : children)
        {
            if (child->GetType() == ASTNodeType::RepeatTemplate)
            {
                auto repeatTemplate = (ASTRepeatTemplate*)child.get();

                auto statements = InstantiateRepeatTemplate(repeatTemplate);
                for (auto& statement : statements)
                {
                    newChildren.push_back(std::move(statement));
                }
            }
            else
            {
                newChildren.push_back(child);
            }
        }

        for (auto& child : newChildren)
        {
            if (child->GetType() == ASTNodeType::TemplateFunction)
            {
                continue;
            }

            InstantiateRepeatTemplates(child.get());
            node->AddChild(child);
        }
    }

    void TemplateInstantiator::InstantiateTemplateFunctions(IASTNode* node)
    {
        auto children = node->GetChildren();
        for (auto& child : children)
        {
            if (child->GetType() == ASTNodeType::FunctionCall)
            {
                auto fnCall = (ASTFunctionCall*)child.get();
                auto& fnName = fnCall->GetFunctionName();
                if (m_TemplateFunctions.find(fnName) == m_TemplateFunctions.end())
                {
                    continue;
                }

                auto templateFn = m_TemplateFunctions[fnName];
                InstantiateTemplateFunction(templateFn, fnCall);
            }
            else
            {
                InstantiateTemplateFunctions(child.get());
            }
        }
    }

    void TemplateInstantiator::InstantiateTemplateFunction(ASTTemplateFunction* templateFunction, ASTFunctionCall* functionCall)
    {
        auto& fnName = templateFunction->GetName();

        auto& templateArgs = templateFunction->GetTemplateArguments();
        std::set<std::string> templateArgsSet;
        for (auto& arg : templateArgs)
        {
            templateArgsSet.insert(arg);
        }

        auto& args = templateFunction->GetArguments();
        auto argsCount = functionCall->GetChildCount();

        if (argsCount != args.size())
        {
            throw TemplateInstantiatorException(SafePrintf("Argument mismatch for template instantiation of \"%\", expected % but got % arguments", fnName, args.size(), argsCount), functionCall);
        }

        auto genFnName = SafePrintf("%__", fnName);

        std::vector<std::shared_ptr<IASTNode>> finalArgs;
        std::vector<std::string> finalArgNames;

        auto& bodyChild = templateFunction->GetChild(0);
        auto body = CloneNode(bodyChild.get());

        for (auto i = 0u; i < args.size(); i++)
        {
            auto& argName = args[i];
            auto& callArg = functionCall->GetArgument(i);
            if (templateArgsSet.count(argName) != 0)
            {
                ReplaceIdentifier(body.get(), argName, callArg);

                if (callArg->GetType() == ASTNodeType::Identifier)
                {
                    auto identifier = (ASTIdentifier*)callArg.get();
                    genFnName += SafePrintf("%_", identifier->GetName());
                }
                else if (callArg->GetType() == ASTNodeType::NumberLiteral)
                {
                    auto number = (ASTNumberLiteral*)callArg.get();
                    genFnName += SafePrintf("%_", number->GetValue());
                }
                else if (callArg->GetType() == ASTNodeType::StringLiteral)
                {
                    auto string = (ASTStringLiteral*)callArg.get();
                    auto hash = std::hash<std::string>()(string->GetValue());
                    genFnName += SafePrintf("[%]_", hash);
                }
                else
                {
                    throw TemplateInstantiatorException(SafePrintf("Invalid template instantiation of \"%\" argument %, template arguments must be either identifiers or number literals", fnName, i), functionCall);
                }
            }
            else
            {
                finalArgs.push_back(callArg);
                finalArgNames.push_back(argName);
            }
        }

        functionCall->SetFunctionName(genFnName);

        functionCall->RemoveChildren();
        for (auto& arg : finalArgs)
        {
            functionCall->AddChild(arg);
        }

        if (m_InstantiatedTemplates.find(genFnName) != m_InstantiatedTemplates.end())
        {
            return;
        }

        auto instantiatedFn = new ASTFunctionDeclaration(genFnName, finalArgNames, templateFunction->GetCharIndex());

        instantiatedFn->AddChild(std::move(body));
        InstantiateTemplateFunctions(instantiatedFn);

        m_Unit->AddChild(std::unique_ptr<IASTNode>(instantiatedFn));

        m_InstantiatedTemplates.insert(std::make_pair(genFnName, instantiatedFn));
    }

    std::vector<std::unique_ptr<IASTNode>> TemplateInstantiator::InstantiateRepeatTemplate(ASTRepeatTemplate* repeatTemplate)
    {
        std::vector<std::unique_ptr<IASTNode>> nodes;

        auto& iterators = repeatTemplate->GetIterators();
        auto itemCount = repeatTemplate->GetArgumentList(iterators[0]).size();

        for (auto i = 0u; i < itemCount; i++)
        {
            auto firstChild = repeatTemplate->GetChild(0);
            if (firstChild->GetType() == ASTNodeType::BlockStatement)
            {
                auto& statements = firstChild->GetChildren();
                for (auto& statement : statements)
                {
                    auto newStatement = CloneNode(statement.get());

                    for (auto& iterator : iterators)
                    {
                        auto& items = repeatTemplate->GetArgumentList(iterator);

                        if (items.size() != itemCount)
                        {
                            throw TemplateInstantiatorException(SafePrintf("Argument list mismatch, expected % items but only got % for iterator \"%\"",
                                itemCount, items.size(), iterator), repeatTemplate);
                        }

                        ReplaceIdentifier(newStatement.get(), iterator, items[i]);
                    }

                    nodes.push_back(std::move(newStatement));
                }
            }
            else
            {
                auto& eventDeclarations = repeatTemplate->GetChildren();
                for (auto& eventDeclaration : eventDeclarations)
                {
                    auto newStatement = CloneNode(eventDeclaration.get());

                    for (auto& iterator : iterators)
                    {
                        auto& items = repeatTemplate->GetArgumentList(iterator);

                        if (items.size() != itemCount)
                        {
                            throw TemplateInstantiatorException(SafePrintf("Argument list mismatch, expected % items but only got % for iterator \"%\"",
                                itemCount, items.size(), iterator), repeatTemplate);
                        }

                        ReplaceIdentifier(newStatement.get(), iterator, items[i]);
                    }

                    nodes.push_back(std::move(newStatement));
                }
            }
        }

        return nodes;
    }

    void TemplateInstantiator::ReplaceIdentifier(IASTNode* node, const std::string& identifierName, const std::shared_ptr<IASTNode>& replaceWith)
    {
        auto children = node->GetChildren();
        node->RemoveChildren();

        for (auto& child : children)
        {
            if (child->GetType() == ASTNodeType::Identifier)
            {
                auto identifier = (ASTIdentifier*)child.get();
                auto& name = identifier->GetName();
                if (name == identifierName)
                {
                    node->AddChild(replaceWith);
                }
                else
                {
                    node->AddChild(child);
                }
            }
            else
            {
                ReplaceIdentifier(child.get(), identifierName, replaceWith);
                node->AddChild(child);
            }
        }
    }

    std::unique_ptr<IASTNode> TemplateInstantiator::CloneNode(IASTNode* node)
    {
        IASTNode* newNode = nullptr;

        switch (node->GetType())
        {
        case ASTNodeType::Unit:
            newNode = new IASTNode(node->GetCharIndex(), ASTNodeType::Unit);
            break;
        case ASTNodeType::FunctionDeclaration:
            newNode = new ASTFunctionDeclaration(*(ASTFunctionDeclaration*)node);
            break;
        case ASTNodeType::TemplateFunction:
            newNode = new ASTTemplateFunction(*(ASTTemplateFunction*)node);
            break;
        case ASTNodeType::BlockStatement:
            newNode = new ASTBlockStatement(*(ASTBlockStatement*)node);
            break;
        case ASTNodeType::Identifier:
            newNode = new ASTIdentifier(*(ASTIdentifier*)node);
            break;
        case ASTNodeType::StringLiteral:
            newNode = new ASTStringLiteral(*(ASTStringLiteral*)node);
            break;
        case ASTNodeType::NumberLiteral:
            newNode = new ASTNumberLiteral(*(ASTNumberLiteral*)node);
            break;
        case ASTNodeType::AssignmentExpression:
            newNode = new ASTAssignmentExpression(*(ASTAssignmentExpression*)node);
            break;
        case ASTNodeType::FunctionCall:
            newNode = new ASTFunctionCall(*(ASTFunctionCall*)node);
            break;
        case ASTNodeType::IfStatement:
            newNode = new ASTIfStatement(*(ASTIfStatement*)node);
            break;
        case ASTNodeType::WhileStatement:
            newNode = new ASTWhileStatement(*(ASTWhileStatement*)node);
            break;
        case ASTNodeType::BinaryExpression:
            newNode = new ASTBinaryExpression(*(ASTBinaryExpression*)node);
            break;
        case ASTNodeType::UnaryExpression:
            newNode = new ASTUnaryExpression(*(ASTUnaryExpression*)node);
            break;
        case ASTNodeType::ReturnStatement:
            newNode = new ASTReturnStatement(*(ASTReturnStatement*)node);
            break;
        case ASTNodeType::VariableDeclaration:
            newNode = new ASTVariableDeclaration(*(ASTVariableDeclaration*)node);
            break;
        case ASTNodeType::EventCondition:
            newNode = new ASTEventCondition(*(ASTEventCondition*)node);
            break;
        case ASTNodeType::EventDeclaration:
            newNode = new ASTEventDeclaration(*(ASTEventDeclaration*)node);
            break;
        case ASTNodeType::ArrayExpression:
            newNode = new ASTArrayExpression(*(ASTArrayExpression*)node);
            break;
        default:
            throw TemplateInstantiatorException("Unsupported AST node type", node);
        }

        newNode->RemoveChildren();

        auto& children = node->GetChildren();
        for (auto& child : children)
        {
            newNode->AddChild(CloneNode(child.get()));
        }

        return std::unique_ptr<IASTNode>(newNode);
    }

}