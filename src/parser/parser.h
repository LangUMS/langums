#ifndef __LANGUMS_PARSER_H
#define __LANGUMS_PARSER_H

#include <memory>
#include <iterator>
#include <string>
#include <sstream>
#include <cctype>

#include "../log.h"
#include "../ast/ast.h"

namespace Langums
{

	enum class TokenType
	{
		BooleanLiteral,
		NumberLiteral,
		StringLiteral,
		Identifier,
		Operator,
		LeftParenthesis,
		RightParenthesis,
		FunctionName,
		FunctionArgumentSeparator
	};

	struct ExpressionToken
	{
		std::string m_Value;
		int m_NumberValue = 0;
		TokenType m_Type;
		OperatorType m_OperatorType;
	};

	class ParserException : public std::exception
	{
		public:
		ParserException(int charPosition, const char* error) : m_CharPosition(charPosition), std::exception(error) {}
		ParserException(int charPosition, const std::string& error) : m_CharPosition(charPosition), std::exception(error.c_str()) {}

		int GetCharPosition() const
		{
			return m_CharPosition;
		}

		private:
		int m_CharPosition = 0;
	};

	class Parser
	{
		public:
		std::unique_ptr<IASTNode> Parse(const std::string& input);

		const std::string& GetSourceBuffer() const
		{
			return m_Buffer;
		}

		private:
		ExpressionToken Token();

		std::unique_ptr<IASTNode> Unit();
		std::unique_ptr<IASTNode> Expression();
		std::unique_ptr<IASTNode> ExpressionTokenToNode(const ExpressionToken& token, std::vector<ExpressionToken>& next);

		std::unique_ptr<IASTNode> ExpressionStatement();
		std::unique_ptr<IASTNode> AssignmentExpression();
		std::unique_ptr<IASTNode> VariableDeclaration();
		std::unique_ptr<IASTNode> ReturnStatement();
		std::unique_ptr<IASTNode> Statement();
		std::unique_ptr<IASTNode> BlockStatement();
		std::unique_ptr<IASTNode> IfStatement();
		std::unique_ptr<IASTNode> WhileStatement();
		std::unique_ptr<IASTNode> FunctionDeclaration();
		std::unique_ptr<IASTNode> GlobalVariableDeclaration();

		std::unique_ptr<IASTNode> EventCondition();
		std::unique_ptr<IASTNode> EventDeclaration();

		OperatorType Operator();
		std::string Identifier();
		int NumberLiteral();
		std::string StringLiteral();

		inline bool IsOperator(char c)
		{
			switch (c)
			{
			case '=': return true;
			case '!': return true;
			case '<': return true;
			case '>': return true;
			case '+': return true;
			case '-': return true;
			case '*': return true;
			case '/': return true;
			case '|': return true;
			case '&': return true;
			}

			return false;
		}

		inline void Symbol(char symbol)
		{
			if (Next() != symbol)
			{
				throw ParserException(m_CurrentChar, SafePrintf("Syntax error. Expected \"%\".", symbol));
			}
		}

		inline bool PeekKeyword(const std::string& keyword)
		{
			auto i = 0u;
			while (i < keyword.length())
			{
				auto c = Peek(i);
				if (c != keyword[i])
				{
					return false;
				}

				i++;
			}

			return true;
		}

		inline void Keyword(const std::string& keyword)
		{
			auto i = 0u;
			while (i < keyword.length())
			{
				auto c = Next();
				if (c != keyword[i])
				{
					throw ParserException(m_CurrentChar, SafePrintf("Syntax error. Expected \"%\".", keyword));
				}

				i++;
			}
		}

		inline void Whitespace()
		{
			if (EndOfStream())
			{
				return;
			}

			while (std::isspace(Peek()))
			{
				Next();

				if (EndOfStream())
				{
					break;
				}
			}
		}

		inline char Peek(int chars = 0)
		{
			auto index = m_CurrentChar + chars;
			if (index >= m_Buffer.size())
			{
				throw ParserException(index, "Unexpected end of input");
			}

			return m_Buffer[index];
		}

		inline char Next()
		{
			if (m_CurrentChar >= m_Buffer.size())
			{
				throw ParserException(m_CurrentChar, "Unexpected end of input");
			}

			return m_Buffer[m_CurrentChar++];
		}

		inline bool EndOfStream()
		{
			return m_CurrentChar >= m_Buffer.size();
		}

		std::string m_Buffer;
		unsigned int m_CurrentChar = 0;
	};

}

#endif
