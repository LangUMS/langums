#include "parser.h"
#include "preprocessor.h"
#include "ast_optimizer.h"

namespace Langums
{

    std::unique_ptr<IASTNode> Parser::Parse(const std::string& input)
    {
        m_Buffer = input;
        m_CurrentChar = 0;
        
        ASTOptimizer optimizer;
        return optimizer.Process(Unit());
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

        if (std::isdigit(c) || (c == '-' && std::isdigit(Peek(1))))
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
                auto functionDef = FunctionDeclaration();
                unit->AddChild(std::move(functionDef));
            }
            else if (PeekKeyword("global"))
            {
                auto globalDeclaration = GlobalVariableDeclaration();
                unit->AddChild(std::move(globalDeclaration));
            }
            else
            {
                auto eventDeclaration = EventDeclaration();
                unit->AddChild(std::move(eventDeclaration));
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
                if (stack[0].m_Type != TokenType::FunctionName)
                {
                    throw ParserException(m_CurrentChar, "Syntax error.");
                }

                stack[0].m_NumberValue++;

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
            auto identifier = new ASTIdentifier(token.m_Value);
            node = std::unique_ptr<IASTNode>(identifier);
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
            auto numberLiteral = new ASTNumberLiteral(token.m_Value == "true" ? 1 : 0);
            node = std::unique_ptr<IASTNode>(numberLiteral);
        }
        else if (token.m_Type == TokenType::NumberLiteral)
        {
            auto numberLiteral = new ASTNumberLiteral(token.m_NumberValue);
            node = std::unique_ptr<IASTNode>(numberLiteral);
        }
        else if (token.m_Type == TokenType::StringLiteral)
        {
            auto stringLiteral = new ASTStringLiteral(token.m_Value);
            node = std::unique_ptr<IASTNode>(stringLiteral);
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
        unaryExpression->AddChild(std::shared_ptr<IASTNode>(new ASTIdentifier(identifier)));
        return std::unique_ptr<IASTNode>(unaryExpression);
    }

    std::unique_ptr<IASTNode> Parser::AssignmentExpression()
    {
        Whitespace();

        auto assignmentExpression = new ASTAssignmentExpression();
        assignmentExpression->AddChild(std::unique_ptr<IASTNode>(new ASTIdentifier(Identifier())));
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

        auto variableDeclaration = new ASTVariableDeclaration(Identifier());

        Whitespace();
        Symbol('=');
        Whitespace();

        variableDeclaration->AddChild(Expression());
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

        auto variableDeclaration = new ASTVariableDeclaration(Identifier());

        Whitespace();
        Symbol('=');
        Whitespace();

        variableDeclaration->AddChild(Expression());

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
            throw ParserException(m_CurrentChar, "Zero length indentifier");
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

        while (true)
        {
            auto c = Peek();
            if (!std::isdigit(c) && c != '-')
            {
                break;
            }

            if (EndOfStream())
            {
                break;
            }

            s.push_back(Next());
        }

        return std::atoi(s.c_str());
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
        while (c != '"')
        {
            value.push_back(Next());
            c = Peek();
        }

        Next();
        return value;
    }

}
