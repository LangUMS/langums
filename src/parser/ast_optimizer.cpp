#include "ast_optimizer.h"

namespace Langums
{

	std::unique_ptr<IASTNode> ASTOptimizer::Process(std::unique_ptr<IASTNode> ast)
	{
		m_Root = std::move(ast);

		auto childCount = m_Root->GetChildCount();
		for (auto i = 0u; i < childCount; i++)
		{
			auto& child = m_Root->GetChild(i);
			m_Root->SetChild(i, CalculateConstantExpressions(child));
		}

		return std::move(m_Root);
	}

	std::shared_ptr<IASTNode> ASTOptimizer::CalculateConstantExpressions(const std::shared_ptr<IASTNode>& node)
	{
		auto childCount = node->GetChildCount();
		for (auto i = 0u; i < childCount; i++)
		{
			auto& child = node->GetChild(i);
			node->SetChild(i, CalculateConstantExpressions(child));
		}

		if (node->GetType() == ASTNodeType::BinaryExpression)
		{
			auto expression = (ASTBinaryExpression*)node.get();
			auto& lhs = expression->GetLHSValue();
			auto& rhs = expression->GetRHSValue();

			if (lhs->GetType() == ASTNodeType::NumberLiteral && rhs->GetType() == ASTNodeType::NumberLiteral)
			{
				auto lhsNumber = (ASTNumberLiteral*)lhs.get();
				auto rhsNumber = (ASTNumberLiteral*)rhs.get();
				auto result = CalculateConstantBinaryExpression(lhsNumber->GetValue(), rhsNumber->GetValue(), expression->GetOperator());
				return std::shared_ptr<IASTNode>(new ASTNumberLiteral(result));
			}

			/*if (expression->GetOperator() == OperatorType::Multiply &&
				(lhs->GetType() == ASTNodeType::NumberLiteral || rhs->GetType() == ASTNodeType::NumberLiteral))
			{
				// turn multiplication by a constant to repeated additions

				ASTNumberLiteral* numberLiteral = (ASTNumberLiteral*)((lhs->GetType() == ASTNodeType::NumberLiteral) ? lhs.get() : rhs.get());
				auto other = (lhs->GetType() == ASTNodeType::NumberLiteral) ? rhs : lhs;

				auto number = numberLiteral->GetValue();
				if (number == 0)
				{
					return std::shared_ptr<IASTNode>(new ASTNumberLiteral(0));
				}

				auto root = std::shared_ptr<IASTNode>(new ASTBinaryExpression(OperatorType::Add));
				auto current = root;

				while (number > 2)
				{
					current->AddChild(other);
					current->AddChild(std::unique_ptr<IASTNode>(new ASTBinaryExpression(OperatorType::Add)));
					current = current->GetChild(1);
					number--;
				}

				current->AddChild(other);
				current->AddChild(other);

				return std::shared_ptr<IASTNode>(root);
			}*/
		}
		else if (node->GetType() == ASTNodeType::UnaryExpression)
		{
			auto expression = (ASTUnaryExpression*)node.get();
			auto& value = expression->GetValue();

			if (value->GetType() == ASTNodeType::NumberLiteral &&
			    expression->GetOperator() == OperatorType::Not)
			{
				auto valueNumber = (ASTNumberLiteral*)value.get();
				auto result = valueNumber->GetValue() > 0 ? 0 : 1;
				return std::shared_ptr<IASTNode>(new ASTNumberLiteral(result));
			}
		}

		return node;
	}

	int ASTOptimizer::CalculateConstantBinaryExpression(int left, int right, OperatorType op)
	{
		switch (op)
		{
			case OperatorType::Or:
				return (left != 0 || right != 0) ? 1 : 0;
			case OperatorType::And:
				return (left != 0 && right != 0) ? 1 : 0;
			case OperatorType::Equals:
				return (left == right) ? 1 : 0;
			case OperatorType::NotEquals:
				return (left != right) ? 1 : 0;
			case OperatorType::GreaterThan:
				return (left > right) ? 1 : 0;
			case OperatorType::GreaterThanOrEquals:
				return (left >= right) ? 1 : 0;
			case OperatorType::LessThan:
				return (left < right) ? 1 : 0;
			case OperatorType::LessThanOrEquals:
				return (left <= right) ? 1 : 0;
			case OperatorType::Add:
				return left + right;
			case OperatorType::Subtract:
				return left - right;
			case OperatorType::Divide:
				return left / right;
			case OperatorType::Multiply:
				return left * right;
		}

		return 0;
	}

}