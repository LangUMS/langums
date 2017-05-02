#include "parser.h"
#include "preprocessor.h"
#include "template_instantiator.h"

namespace Langums
{

    std::unique_ptr<IASTNode> Parser::Parse(const std::string& input)
    {
        m_Buffer = input;
        m_CurrentChar = 0;

        auto unit = Unit();
        TemplateInstantiator templateInstantiator;
        templateInstantiator.Process(unit);
        return unit;
    }

    ExpressionToken Parser::Token()
    {
        auto c = Peek();

        if (c == '(')
        {
            Next();
            ExpressionToken token;
            token.m_Type = TokenType::LeftParenthesis;
            return token;
        }

        if (c == ')')
        {
            Next();
            ExpressionToken token;
            token.m_Type = TokenType::RightParenthesis;
            return token;
        }

        if (c == '[')
        {
            Next();
            ExpressionToken token;
            token.m_Type = TokenType::LeftSquareBracket;
            return token;
        }

        if (c == ']')
        {
            Next();
            ExpressionToken token;
            token.m_Type = TokenType::RightSquareBracket;
            return token;
        }

        if (std::isdigit(c) || (c == '-' && std::isdigit(Peek(1))) || PeekKeyword("0x"))
        {
            ExpressionToken token;
            token.m_Type = TokenType::NumberLiteral;
            token.m_NumberValue = NumberLiteral();
            return token;
        }

        if (c == '"')
        {
            ExpressionToken token;
            token.m_Type = TokenType::StringLiteral;
            token.m_Value = StringLiteral();
            return token;
        }

        if (IsOperator(c))
        {
            ExpressionToken token;
            token.m_Type = TokenType::Operator;
            token.m_OperatorType = Operator();
            return token;
        }

        if (c == ',')
        {
            ExpressionToken token;
            token.m_Type = TokenType::FunctionArgumentSeparator;
            Next();
            return token;
        }

        ExpressionToken token;
        token.m_Type = TokenType::Identifier;
        token.m_Value = Identifier();

        Whitespace();
        if (Peek() == '(')
        {
            token.m_Type = TokenType::FunctionName;
        }

        if (token.m_Value == "true" || token.m_Value == "false")
        {
            token.m_Type = TokenType::BooleanLiteral;
        }

        return token;
    }

    std::unique_ptr<IASTNode> Parser::Unit()
    {
        auto unit = std::make_unique<IASTNode>(ASTNodeType::Unit);

        while (!EndOfStream())
        {
            Whitespace();
            if (EndOfStream())
            {
                break;
            }

            if (PeekKeyword("fn"))
            {
                auto i = 0;
                auto c = Peek(i);
                while (c != '(' && c != '<')
                {
                    i++;
                    c = Peek(i);
                }

                if (c == '(')
                {
                    unit->AddChild(FunctionDeclaration());
                }
                else if(c == '<')
                {
                    unit->AddChild(TemplateFunction());
                }
                else
                {
                    throw ParserException(m_CurrentChar, "Unexpected input");
                }
            }
            else if (PeekKeyword("global"))
            {
                unit->AddChild(GlobalVariableDeclaration());
            }
            else if (PeekKeyword("unit"))
            {
                unit->AddChild(UnitProperties());
            }
            else if (PeekKeyword("for"))
            {
                unit->AddChild(EventTemplateBlock());
            }
            else
            {
                unit->AddChild(EventDeclaration());
            }
        }

        return unit;
    }

    OperatorType Parser::Operator()
    {
        auto next = Next();

        auto c = Peek();

        if (next == '=' && c == '=')
        {
            Next();
            return OperatorType::Equals;
        }

        if (next == '!' && c == '=')
        {
            Next();
            return OperatorType::NotEquals;
        }
        else if (next == '!')
        {
            return OperatorType::Not;
        }

        if (next == '<' && c == '=')
        {
            Next();
            return OperatorType::LessThanOrEquals;
        }
        else if (next == '<')
        {
            return OperatorType::LessThan;
        }

        if (next == '>' && c == '=')
        {
            Next();
            return OperatorType::GreaterThanOrEquals;
        }
        else if (next == '>')
        {
            return OperatorType::GreaterThan;
        }

        if (next == '|' && c == '|')
        {
            Next();
            return OperatorType::Or;
        }

        if (next == '&' && c == '&')
        {
            Next();
            return OperatorType::And;
        }

        if (next == '+' && c == '+')
        {
            Next();
            return OperatorType::PostfixIncrement;
        }

        if (next == '-' && c == '-')
        {
            Next();
            return OperatorType::PostfixDecrement;
        }

        switch (next)
        {
        case '+': return OperatorType::Add;
        case '-': return OperatorType::Subtract;
        case '*': return OperatorType::Multiply;
        case '/': return OperatorType::Divide;
        }

        throw ParserException(m_CurrentChar, "Invalid operator");
    }

    std::unique_ptr<IASTNode> Parser::Expression()
    {
        std::vector<ExpressionToken> stack;
        std::vector<ExpressionToken> output;

        int insideFunctionCall = 0;

        while (true)
        {
            auto c = Peek();
            if (c == ';' || c == '{')
            {
                break;
            }

            auto token = Token();
            Whitespace();

            switch (token.m_Type)
            {
            case TokenType::BooleanLiteral:
            case TokenType::StringLiteral:
            case TokenType::NumberLiteral:
            case TokenType::Identifier:
                output.push_back(token);
                continue;
            }

            if (token.m_Type == TokenType::FunctionName)
            {
                stack.push_back(token);
            }
            if (token.m_Type == TokenType::FunctionArgumentSeparator)
            {
                auto fnIndex = (int)stack.size() - 1;
                while (stack[fnIndex].m_Type != TokenType::FunctionName)
                {
                    fnIndex--;
                    if (fnIndex < 0)
                    {
                        throw ParserException(m_CurrentChar, "Syntax error");
                    }
                }

                stack[fnIndex].m_NumberValue++;

                while (stack.back().m_Type != TokenType::LeftParenthesis)
                {
                    output.push_back(stack.back());
                    stack.pop_back();

                    if (stack.size() == 0)
                    {
                        throw ParserException(m_CurrentChar, "Mismatched parenthesis");
                    }
                }
            }
            else if (token.m_Type == TokenType::Operator)
            {
                auto o1 = token.m_OperatorType;

                while (stack.size() > 0 && stack.back().m_Type == TokenType::Operator)
                {
                    auto o2 = stack.back().m_OperatorType;
                    if ((int)o1 <= (int)o2)
                    {
                        output.push_back(stack.back());
                        stack.pop_back();
                    }
                    else
                    {
                        break;
                    }
                }

                while (stack.size() > 0 && stack.back().m_Type == TokenType::FunctionName)
                {
                    output.push_back(stack.back());
                    stack.pop_back();
                }

                stack.push_back(token);
            }
            else if (token.m_Type == TokenType::LeftParenthesis)
            {
                if (stack.size() > 0 && stack.back().m_Type == TokenType::FunctionName || insideFunctionCall > 0)
                {
                    insideFunctionCall++;

                    Whitespace();
                    if (Peek() != ')')
                    {
                        stack.back().m_NumberValue++;
                    }
                }

                stack.push_back(token);
            }
            else if (token.m_Type == TokenType::RightParenthesis)
            {
                if (insideFunctionCall > 0)
                {
                    insideFunctionCall--;
                }

                while (stack.size() > 0 && stack.back().m_Type != TokenType::LeftParenthesis)
                {
                    output.push_back(stack.back());
                    stack.pop_back();
                }

                if (stack.size() < 1)
                {
                    throw ParserException(m_CurrentChar, "Mismatched parenthesis");
                }

                auto& back = stack.back();

                if (back.m_Type == TokenType::Identifier || back.m_Type == TokenType::FunctionName)
                {
                    output.push_back(stack.back());
                }

                stack.pop_back();
            }
            else if (token.m_Type == TokenType::LeftSquareBracket)
            {
                if (output.size() == 0)
                {
                    throw ParserException(m_CurrentChar, "Syntax error. Unexpected \"[\"");
                }

                if (output.back().m_Type != TokenType::Identifier)
                {
                    throw ParserException(m_CurrentChar, "Syntax error. Unexpected \"[\"");
                }

                auto identifier = output.back();
                output.pop_back();

                if (std::isdigit(Peek()))
                {
                    auto arrayIndex = NumberLiteral();
                    if (arrayIndex < 0)
                    {
                        throw ParserException(m_CurrentChar, SafePrintf("Invalid array index %", arrayIndex));
                    }

                    token.m_NumberValue = arrayIndex;
                }
                else
                {
                    token.m_NumberValue = -1;
                    token.m_Value = Identifier();
                }

                token.m_ArrayIdentifier = identifier.m_Value;
                token.m_Type = TokenType::ArrayOperator;
                output.push_back(token);

                Whitespace();
                Symbol(']');
                Whitespace();
            }
            else if (token.m_Type == TokenType::RightSquareBracket)
            {
                throw ParserException(m_CurrentChar, "Syntax error. Unexpected \"]\"");
            }
        }

        while (stack.size() > 0)
        {
            auto token = stack.back();
            stack.pop_back();

            if (token.m_Type == TokenType::LeftParenthesis || token.m_Type == TokenType::RightParenthesis)
            {
                throw ParserException(m_CurrentChar, "Mismatched parenthesis");
            }

            output.push_back(token);
        }

        if (output.size() == 0)
        {
            throw ParserException(m_CurrentChar, "Empty expression");
        }

        auto token = output.back();
        output.pop_back();
        return ExpressionTokenToNode(token, output);
    }

    std::unique_ptr<IASTNode> Parser::ExpressionTokenToNode(const ExpressionToken& token, std::vector<ExpressionToken>& next)
    {
        std::unique_ptr<IASTNode> node = nullptr;

        if (token.m_Type == TokenType::Operator)
        {
            if (token.m_OperatorType == OperatorType::Not ||
                token.m_OperatorType == OperatorType::PostfixIncrement ||
                token.m_OperatorType == OperatorType::PostfixDecrement)
            {
                if (next.size() == 0)
                {
                    throw ParserException(m_CurrentChar, "Missing value in unary expression");
                }

                auto nxt = next.back();
                next.pop_back();

                auto unaryExpression = new ASTUnaryExpression(token.m_OperatorType);
                unaryExpression->AddChild(ExpressionTokenToNode(nxt, next));
                node = std::unique_ptr<IASTNode>(unaryExpression);
            }
            else
            {
                if (next.size() < 2)
                {
                    throw ParserException(m_CurrentChar, "Missing value in binary expression");
                }

                auto nxt = next.back();
                next.pop_back();

                auto binaryExpression = new ASTBinaryExpression(token.m_OperatorType);
                binaryExpression->AddChild(ExpressionTokenToNode(nxt, next));

                nxt = next.back();
                next.pop_back();
                binaryExpression->AddChild(ExpressionTokenToNode(nxt, next));
                node = std::unique_ptr<IASTNode>(binaryExpression);
            }
        }
        else if (token.m_Type == TokenType::Identifier)
        {
            node = std::unique_ptr<IASTNode>(new ASTIdentifier(token.m_Value));
        }
        else if (token.m_Type == TokenType::FunctionName)
        {
            auto argCount = token.m_NumberValue;

            auto functionCall = new ASTFunctionCall(token.m_Value);
            node = std::unique_ptr<IASTNode>(functionCall);

            for (auto i = 0; i < argCount; i++)
            {
                auto nxt = next.back();
                next.pop_back();

                node->AddChild(ExpressionTokenToNode(nxt, next));
            }
        }
        else if (token.m_Type == TokenType::BooleanLiteral)
        {
            node = std::unique_ptr<IASTNode>(new ASTNumberLiteral(token.m_Value == "true" ? 1 : 0));
        }
        else if (token.m_Type == TokenType::NumberLiteral)
        {
            node = std::unique_ptr<IASTNode>(new ASTNumberLiteral(token.m_NumberValue));
        }
        else if (token.m_Type == TokenType::StringLiteral)
        {
            node = std::unique_ptr<IASTNode>(new ASTStringLiteral(token.m_Value));
        }
        else if (token.m_Type == TokenType::ArrayOperator)
        {
            if (token.m_NumberValue != -1)
            {
                auto arrayExpression = new ASTArrayExpression(token.m_ArrayIdentifier);
                arrayExpression->AddChild(std::unique_ptr<IASTNode>(new ASTNumberLiteral(token.m_NumberValue)));
                node = std::unique_ptr<IASTNode>(arrayExpression);
            }
            else
            {
                auto arrayExpression = new ASTArrayExpression(token.m_ArrayIdentifier);
                arrayExpression->AddChild(std::unique_ptr<IASTNode>(new ASTIdentifier(token.m_Value)));
                node = std::unique_ptr<IASTNode>(arrayExpression);
            }
        }
        else
        {
            throw ParserException(m_CurrentChar, "Invalid token type in expression");
        }

        return node;
    }

    std::unique_ptr<IASTNode> Parser::ExpressionStatement()
    {
        Whitespace();

        auto identifier = Identifier();
        Whitespace();

        std::unique_ptr<IASTNode> arrayIndex = nullptr;
        if (Peek() == '[')
        {
            Symbol('[');
            Whitespace();

            arrayIndex = Expression();

            Whitespace();
            Symbol(']');
            Whitespace();
        }

        OperatorType op;

        if (PeekKeyword("++"))
        {
            Keyword("++");
            op = OperatorType::PostfixIncrement;
        }
        else if (PeekKeyword("--"))
        {
            Keyword("--");
            op = OperatorType::PostfixDecrement;
        }
        else
        {
            throw new ParserException(m_CurrentChar, "Invalid expression statement");
        }

        Whitespace();

        auto unaryExpression = new ASTUnaryExpression(op);

        if (arrayIndex == nullptr)
        {
            unaryExpression->AddChild(std::shared_ptr<IASTNode>(new ASTIdentifier(identifier)));
        }
        else
        {
            auto arrayExpression = new ASTArrayExpression(identifier);
            arrayExpression->AddChild(std::move(arrayIndex));
            unaryExpression->AddChild(std::shared_ptr<IASTNode>(arrayExpression));
        }

        return std::unique_ptr<IASTNode>(unaryExpression);
    }

    std::unique_ptr<IASTNode> Parser::AssignmentExpression()
    {
        Whitespace();

        auto assignmentExpression = new ASTAssignmentExpression();

        auto i = 0;
        while (Peek(i) != '=' && Peek(i) != '[')
        {
            i++;
        }

        auto c = Peek(i);
        if (c == '[')
        {
            auto identifier = Identifier();
            Whitespace();
            Symbol('[');
            Whitespace();

            auto arrayExpression = new ASTArrayExpression(identifier);

            if (std::isdigit(Peek()))
            {
                arrayExpression->AddChild(std::unique_ptr<IASTNode>(new ASTNumberLiteral(NumberLiteral())));
            }
            else
            {
                arrayExpression->AddChild(std::unique_ptr<IASTNode>(new ASTIdentifier(Identifier())));
            }

            Whitespace();
            Symbol(']');

            assignmentExpression->AddChild(std::unique_ptr<IASTNode>(arrayExpression));
        }
        else
        {
            assignmentExpression->AddChild(std::unique_ptr<IASTNode>(new ASTIdentifier(Identifier())));
        }

        Whitespace();
        Symbol('=');
        Whitespace();
        assignmentExpression->AddChild(Expression());

        return std::unique_ptr<IASTNode>(assignmentExpression);
    }

    std::unique_ptr<IASTNode> Parser::VariableDeclaration()
    {
        Keyword("var");
        Whitespace();

        auto name = Identifier();

        Whitespace();

        auto arraySize = 1;
        std::unique_ptr<IASTNode> assignmentExpression;

        if (Peek() == '=')
        {
            Symbol('=');
            Whitespace();
            assignmentExpression = Expression();
        }
        else if (Peek() == '[')
        {
            Symbol('[');
            Whitespace();

            arraySize = NumberLiteral();
            if (arraySize <= 0)
            {
                throw ParserException(m_CurrentChar, SafePrintf("Invalid array size %", arraySize));
            }

            Whitespace();
            Symbol(']');
        }

        auto variableDeclaration = new ASTVariableDeclaration(name, arraySize);

        if (assignmentExpression != nullptr)
        {
            variableDeclaration->AddChild(std::move(assignmentExpression));
        }

        return std::unique_ptr<IASTNode>(variableDeclaration);
    }

    std::unique_ptr<IASTNode> Parser::ReturnStatement()
    {
        Keyword("return");
        Whitespace();
        auto returnStatement = new ASTReturnStatement();

        if (Peek() != ';')
        {
            returnStatement->AddChild(Expression());
            Whitespace();
        }

        return std::unique_ptr<IASTNode>(returnStatement);
    }

    std::unique_ptr<IASTNode> Parser::Statement()
    {
        std::unique_ptr<IASTNode> statement = nullptr;

        Whitespace();

        if (PeekKeyword("var"))
        {
            statement = VariableDeclaration();
            Symbol(';');
            Whitespace();
            return statement;
        }

        if (PeekKeyword("return"))
        {
            statement = ReturnStatement();
            Symbol(';');
            Whitespace();
            return statement;
        }

        int i = 0;
        auto c = Peek(0);
        while (c != '(' && c != '=' && c != '+' && c != '-')
        {
            i++;
            c = Peek(i);
        }

        bool requiresSemicolon = true;

        if (c == '(')
        {
            if (PeekKeyword("if"))
            {
                statement = IfStatement();
                requiresSemicolon = false;
            }
            else if (PeekKeyword("while"))
            {
                statement = WhileStatement();
                requiresSemicolon = false;
            }
            else
            {
                statement = Expression();
            }
        }
        else if (c == '=')
        {
            statement = AssignmentExpression();
        }
        else if ((c == '+' && Peek(i + 1) == '+') || (c == '-' && Peek(i + 1) == '-'))
        {
            statement = ExpressionStatement();
        }
        else
        {
            throw ParserException(m_CurrentChar, "Invalid statement");
        }

        Whitespace();

        if (requiresSemicolon)
        {
            Symbol(';');
            Whitespace();
        }

        return statement;
    }

    std::unique_ptr<IASTNode> Parser::BlockStatement()
    {
        Whitespace();
        Symbol('{');

        auto blockStatement = new ASTBlockStatement();
        
        Whitespace();

        while (true)
        {
            auto c = Peek();
            if (c == '}')
            {
                break;
            }

            blockStatement->AddChild(Statement());
        }

        Symbol('}');

        return std::unique_ptr<IASTNode>(blockStatement);
    }
    
    std::unique_ptr<IASTNode> Parser::IfStatement()
    {
        Keyword("if");
        Whitespace();

        auto ifStatement = new ASTIfStatement();
        ifStatement->AddChild(Expression());
        Whitespace();
        ifStatement->AddChild(BlockStatement());
        Whitespace();

        if (PeekKeyword("else"))
        {
            Keyword("else");
            ifStatement->AddChild(BlockStatement());
        }

        return std::unique_ptr<IASTNode>(ifStatement);
    }

    std::unique_ptr<IASTNode> Parser::WhileStatement()
    {
        Keyword("while");
        Whitespace();

        auto whileStatement = new ASTWhileStatement();
        whileStatement->AddChild(Expression());
        Whitespace();
        whileStatement->AddChild(BlockStatement());

        return std::unique_ptr<IASTNode>(whileStatement);
    }

    std::unique_ptr<IASTNode> Parser::TemplateFunction()
    {
        Whitespace();
        Keyword("fn");
        Whitespace();

        auto functionName = Identifier();

        Symbol('<');
        Whitespace();

        std::vector<std::string> templateArgs;

        while (true)
        {
            auto c = Peek();
            if (c == '>')
            {
                break;
            }

            templateArgs.push_back(Identifier());
            Whitespace();

            c = Peek();
            if (c == ',')
            {
                Next();
                Whitespace();
            }
        }

        Symbol('>');
        Whitespace();
        Symbol('(');
        Whitespace();

        std::vector<std::string> args;

        while (true)
        {
            auto c = Peek();
            if (c == ')')
            {
                break;
            }

            args.push_back(Identifier());
            Whitespace();

            c = Peek();
            if (c == ',')
            {
                Next();
                Whitespace();
            }
        }

        Whitespace();
        Symbol(')');
        Whitespace();

        auto templateDeclaration = new ASTTemplateFunction(functionName, args, templateArgs);
        templateDeclaration->AddChild(BlockStatement());
        Whitespace();

        return std::unique_ptr<IASTNode>(templateDeclaration);
    }

    std::unique_ptr<IASTNode> Parser::FunctionDeclaration()
    {
        Whitespace();
        Keyword("fn");
        Whitespace();

        auto functionName = Identifier();

        Symbol('(');
        Whitespace();

        std::vector<std::string> args;

        while (true)
        {
            auto c = Peek();
            if (c == ')')
            {
                break;
            }

            args.push_back(Identifier());
            Whitespace();

            c = Peek();
            if (c == ',')
            {
                Next();
                Whitespace();
            }
        }

        Whitespace();
        Symbol(')');
        Whitespace();

        auto functionDeclaration = new ASTFunctionDeclaration(functionName, args);
        functionDeclaration->AddChild(BlockStatement());
        Whitespace();

        return std::unique_ptr<IASTNode>(functionDeclaration);
    }

    std::unique_ptr<IASTNode> Parser::GlobalVariableDeclaration()
    {
        Keyword("global");
        Whitespace();

        auto name = Identifier();

        Whitespace();

        auto arraySize = 1;
        std::unique_ptr<IASTNode> assignmentExpression;

        if (Peek() == '=')
        {
            Symbol('=');
            Whitespace();
            assignmentExpression = Expression();
        }
        else if (Peek() == '[')
        {
            Symbol('[');
            Whitespace();

            arraySize = NumberLiteral();
            if (arraySize <= 0)
            {
                throw ParserException(m_CurrentChar, SafePrintf("Invalid array size %", arraySize));
            }

            Whitespace();
            Symbol(']');
        }

        auto variableDeclaration = new ASTVariableDeclaration(name, arraySize);

        if (assignmentExpression != nullptr)
        {
            variableDeclaration->AddChild(std::move(assignmentExpression));
        }
       
        Whitespace();

        Symbol(';');

        return std::unique_ptr<IASTNode>(variableDeclaration);
    }

    std::unique_ptr<IASTNode> Parser::EventCondition()
    {
        auto conditionName = Identifier();
        auto condition = new ASTEventCondition(conditionName);

        Whitespace();
        Symbol('(');
        Whitespace();

        auto c = Peek();
        while (c != ')')
        {
            if (c == '"')
            {
                condition->AddChild(std::shared_ptr<IASTNode>(new ASTStringLiteral(StringLiteral())));
            }
            else if (std::isdigit(c))
            {
                condition->AddChild(std::shared_ptr<IASTNode>(new ASTNumberLiteral(NumberLiteral())));
            }
            else
            {
                condition->AddChild(std::shared_ptr<IASTNode>(new ASTIdentifier(Identifier())));
            }
            
            Whitespace();

            c = Peek();
            if (c == ')')
            {
                break;
            }

            Symbol(',');
            Whitespace();
            c = Peek();
        }

        Symbol(')');

        return std::unique_ptr<IASTNode>(condition);
    }

    std::unique_ptr<IASTNode> Parser::EventDeclaration()
    {
        auto eventDeclaration = new ASTEventDeclaration();

        eventDeclaration->AddChild(EventCondition());
        Whitespace();

        auto c = Peek();
        while (c == ',')
        {
            Next();
            Whitespace();
            eventDeclaration->AddChild(EventCondition());
            Whitespace();
            c = Peek();
        }

        Whitespace();
        Keyword("=>");
        Whitespace();

        eventDeclaration->AddChild(BlockStatement());

        return std::unique_ptr<IASTNode>(eventDeclaration);
    }

    std::unique_ptr<IASTNode> Parser::EventTemplateBlock()
    {
        Keyword("for");
        Whitespace();
        Symbol('<');
        Whitespace();

        auto iteratorName = Identifier();

        Whitespace();
        Symbol('>');
        Whitespace();

        Keyword("in");
        Whitespace();

        Symbol('(');
        Whitespace();

        std::vector<std::string> list;
        list.push_back(Identifier());
        Whitespace();

        auto c = Peek();
        while (c == ',')
        {
            Next();
            Whitespace();
            list.push_back(Identifier());
            Whitespace();
            c = Peek();
        }

        auto eventTemplateBlock = new ASTEventTemplateBlock(iteratorName, list);

        Symbol(')');
        Whitespace();

        Symbol('{');
        Whitespace();

        c = Peek();
        while (c != '}')
        {
            eventTemplateBlock->AddChild(EventDeclaration());
            Whitespace();
            c = Peek();
        }

        Symbol('}');
        Whitespace();
        return std::unique_ptr<IASTNode>(eventTemplateBlock);
    }

    std::unique_ptr<IASTNode> Parser::UnitProperties()
    {
        Keyword("unit");
        Whitespace();

        auto name = Identifier();

        Whitespace();
        Symbol('{');
        Whitespace();

        auto unitProperties = new ASTUnitProperties(name);

        unitProperties->AddChild(UnitProperty());
        Whitespace();

        auto c = Peek();
        while (c == ',')
        {
            Next();
            Whitespace();
            unitProperties->AddChild(UnitProperty());
            Whitespace();
            c = Peek();
        }

        Symbol('}');
        Whitespace();

        return std::unique_ptr<IASTNode>(unitProperties);
    }

    std::unique_ptr<IASTNode> Parser::UnitProperty()
    {
        auto name = Identifier();
        Whitespace();
        Symbol('=');
        Whitespace();

        auto c = Peek();

        if (PeekKeyword("true"))
        {
            Keyword("true");
            return std::unique_ptr<IASTNode>(new ASTUnitProperty(name, 1));
        }

        if (PeekKeyword("false"))
        {
            Keyword("false");
            return std::unique_ptr<IASTNode>(new ASTUnitProperty(name, 0));
        }

        return std::unique_ptr<IASTNode>(new ASTUnitProperty(name, NumberLiteral()));
    }

    std::string Parser::Identifier()
    {
        std::string identifier;
        static std::string validIdentifierChars = "abcdefghijklmnopqrstuvwxyzABCEDFGHIJKLMNOPQRSTUVWXYZ0123456789_";

        auto c = Peek();
        while (validIdentifierChars.find(c) != std::string::npos)
        {
            identifier.push_back(Next());

            if (EndOfStream())
            {
                break;
            }

            c = Peek();
        }

        if (identifier.length() == 0)
        {
            throw ParserException(m_CurrentChar, "Syntax error");
        }

        if (std::isdigit(identifier[0]))
        {
            throw ParserException(m_CurrentChar, SafePrintf("Invalid identifier \"%\"", identifier));
        }

        return identifier;
    }

    int Parser::NumberLiteral()
    {
        std::string s;

        auto isHex = false;
        if (PeekKeyword("0x"))
        {
            Keyword("0x");
            isHex = true;
        }

        while (true)
        {
            auto c = Peek();

            if (isHex)
            {
                c = std::tolower(c);
            }

            if ((!isHex && (!std::isdigit(c) && c != '-')) || (isHex && !std::isxdigit(c)))
            {
                break;
            }

            if (EndOfStream())
            {
                break;
            }

            s.push_back(Next());
        }

        if (!isHex)
        {
            return std::atoi(s.c_str());
        }
        else
        {
            std::stringstream toHex;
            toHex << std::hex << s;
            unsigned int value;
            toHex >> value;
            return value;
        }
    }

    std::string Parser::StringLiteral()
    {
        auto next = Next();
        if (next != '"')
        {
            throw ParserException(m_CurrentChar, "Invalid string value, expected '\"'");
        }

        std::string value;

        auto c = Peek();
        if (c == '"' && Peek(1) == '"')
        {
            Next();
            Next();

            // multi-line string

            while (!PeekKeyword("\"\"\""))
            {
                value.push_back(Next());
            }

            Keyword("\"\"\"");
        }
        else
        {
            while (c != '"')
            {
                value.push_back(Next());
                c = Peek();
                if (c == '\n')
                {
                    throw ParserException(m_CurrentChar, "Invalid string value, expected '\"'");
                }
            }

            Next();
        }
     
        return value;
    }

}
