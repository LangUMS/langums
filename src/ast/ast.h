#ifndef __AST_ASTNODE_H
#define __AST_ASTNODE_H

#include <memory>
#include <vector>

namespace Langums
{

    enum class OperatorType
    {
        Or,
        And,
        Equals,
        NotEquals,
        GreaterThan,
        GreaterThanOrEquals,
        LessThan,
        LessThanOrEquals,
        Add,
        Subtract,
        Divide,
        Multiply,
        Not,
        PostfixIncrement,
        PostfixDecrement
    };

    enum class ASTNodeType
    {
        Unit = 0,
        FunctionDeclaration,
        TemplateFunction,
        BlockStatement,
        Identifier,
        StringLiteral,
        NumberLiteral,
        AssignmentExpression,
        FunctionCall,
        IfStatement,
        WhileStatement,
        BinaryExpression,
        ArrayExpression,
        UnaryExpression,
        ReturnStatement,
        VariableDeclaration,
        EventCondition,
        EventDeclaration,
        RepeatTemplate,
        UnitProperties,
        UnitProperty
    };

    inline std::string ASTNodeTypeToName(ASTNodeType nodeType)
    {
        switch (nodeType)
        {
            case ASTNodeType::Unit:
                return "Unit";
            case ASTNodeType::FunctionDeclaration:
                return "FunctionDeclaration";
            case ASTNodeType::TemplateFunction:
                return "TemplateFunction";
            case ASTNodeType::BlockStatement:
                return "BlockStatement";
            case ASTNodeType::Identifier:
                return "Identifier";
            case ASTNodeType::StringLiteral:
                return "StringLiteral";
            case ASTNodeType::NumberLiteral:
                return "NumberLiteral";
            case ASTNodeType::AssignmentExpression:
                return "AssignmentExpression";
            case ASTNodeType::FunctionCall:
                return "FunctionCall";
            case ASTNodeType::IfStatement:
                return "IfStatement";
            case ASTNodeType::WhileStatement:
                return "WhileStatement";
            case ASTNodeType::BinaryExpression:
                return "BinaryExpression";
            case ASTNodeType::ArrayExpression:
                return "ArrayExpression";
            case ASTNodeType::UnaryExpression:
                return "UnaryExpression";
            case ASTNodeType::ReturnStatement:
                return "ReturnStatement";
            case ASTNodeType::VariableDeclaration:
                return "VariableDeclaration";
            case ASTNodeType::EventCondition:
                return "EventCondition";
            case ASTNodeType::EventDeclaration:
                return "EventDeclaration";
            case ASTNodeType::RepeatTemplate:
                return "RepeatTemplate";
            case ASTNodeType::UnitProperties:
                return "UnitProperties";
            case ASTNodeType::UnitProperty:
                return "UnitProperty";
        }

        return "UnknownType";
    }

    class IASTNode
    {
        public:
        IASTNode(unsigned int charIndex, ASTNodeType type) : m_CharIndex(charIndex), m_Type(type) {}
        virtual ~IASTNode() {}

        ASTNodeType GetType() const
        {
            return m_Type;
        }

        std::string GetTypeName() const
        {
            return ASTNodeTypeToName(m_Type);
        }

        unsigned int GetCharIndex() const
        {
            return m_CharIndex;
        }
        
        void AddChild(const std::shared_ptr<IASTNode>& node)
        {
            node->m_Parent = this;
            m_Children.push_back(node);
        }

        void RemoveChild(size_t index)
        {
            m_Children[index]->m_Parent = nullptr;
            m_Children.erase(m_Children.begin() + index);
        }

        void RemoveChildren()
        {
            m_Children.clear();
        }

        void SetChild(size_t index, const std::shared_ptr<IASTNode>& node)
        {
            m_Children[index] = node;
            node->m_Parent = this;
        }

        bool HasChildren() const
        {
            return !m_Children.empty();
        }

        size_t GetChildCount() const
        {
            return m_Children.size();
        }

        const std::shared_ptr<IASTNode>& GetChild(size_t index) const
        {
            if (index >= m_Children.size())
            {
                return m_EmptyChild;
            }

            return m_Children[index];
        }

        const std::vector<std::shared_ptr<IASTNode>>& GetChildren() const
        {
            return m_Children;
        }

        IASTNode* GetParent() const
        {
            return m_Parent;
        }

        private:
        IASTNode* m_Parent = nullptr;

        ASTNodeType m_Type = ASTNodeType::Unit;
        std::vector<std::shared_ptr<IASTNode>> m_Children;
        std::shared_ptr<IASTNode> m_EmptyChild = nullptr;
        unsigned int m_CharIndex;
    };

    class ASTStringLiteral : public IASTNode
    {
        public:
        ASTStringLiteral(const std::string& value, unsigned int charIndex) :
            m_Value(value), IASTNode(charIndex, ASTNodeType::StringLiteral) {}

        const std::string& GetValue() const
        {
            return m_Value;
        }

        private:
        std::string m_Value;
    };

    class ASTNumberLiteral : public IASTNode
    {
        public:
        ASTNumberLiteral(int value, unsigned int charIndex) :
            m_Value(value), IASTNode(charIndex, ASTNodeType::NumberLiteral) {}

        int GetValue() const
        {
            return m_Value;
        }

        private:
        int m_Value;
    };

    class ASTIdentifier : public IASTNode
    {
        public:
        ASTIdentifier(const std::string& name, unsigned int charIndex) :
            m_Name(name), IASTNode(charIndex, ASTNodeType::Identifier) {}

        const std::string& GetName() const
        {
            return m_Name;
        }

        void SetName(const std::string& name)
        {
            m_Name = name;
        }

        private:
        std::string m_Name;
    };

    class ASTAssignmentExpression : public IASTNode
    {
        public:
        ASTAssignmentExpression(unsigned int charIndex) : IASTNode(charIndex, ASTNodeType::AssignmentExpression) {}

        const std::shared_ptr<IASTNode>& GetLHSValue() const
        {
            return GetChild(0);
        }

        const std::shared_ptr<IASTNode>& GetRHSValue() const
        {
            return GetChild(1);
        }
    };

    class ASTBinaryExpression : public IASTNode
    {
        public:
        ASTBinaryExpression(OperatorType op, unsigned int charIndex) :
            m_Operator(op), IASTNode(charIndex, ASTNodeType::BinaryExpression) {}

        const std::shared_ptr<IASTNode>& GetLHSValue() const
        {
            return GetChild(0);
        }

        const std::shared_ptr<IASTNode>& GetRHSValue() const
        {
            return GetChild(1);
        }

        OperatorType GetOperator() const
        {
            return m_Operator;
        }
        
        private:
        OperatorType m_Operator = OperatorType::Add;
    };

    class ASTArrayExpression : public IASTNode
    {
        public:
        ASTArrayExpression(const std::string& identifier, unsigned int charIndex) :
            m_Identifier(identifier), IASTNode(charIndex, ASTNodeType::ArrayExpression)
        {}

        const std::string& GetIdentifier() const
        {
            return m_Identifier;
        }

        const std::shared_ptr<IASTNode>& GetIndex() const
        {
            return GetChild(0);
        }

        private:
        std::string m_Identifier;
    };

    class ASTUnaryExpression : public IASTNode
    {
        public:
        ASTUnaryExpression(OperatorType op, unsigned int charIndex) :
            m_Operator(op), IASTNode(charIndex, ASTNodeType::UnaryExpression) {}

        const std::shared_ptr<IASTNode>& GetValue() const
        {
            return GetChild(0);
        }

        OperatorType GetOperator() const
        {
            return m_Operator;
        }

        private:
        OperatorType m_Operator = OperatorType::Not;
    };

    class ASTFunctionCall : public IASTNode
    {
        public:
        ASTFunctionCall(const std::string& fnName, unsigned int charIndex) :
            m_FunctionName(fnName), IASTNode(charIndex, ASTNodeType::FunctionCall) {}

        const std::string& GetFunctionName() const
        {
            return m_FunctionName;
        }
        
        void SetFunctionName(const std::string& fnName)
        {
            m_FunctionName = fnName;
        }

        const std::shared_ptr<IASTNode>& GetArgument(int index) const
        {
            auto args = GetChildCount();
            return GetChild(args - index - 1);
        }

        private:
        std::string m_FunctionName;
    };

    class ASTIfStatement : public IASTNode
    {
        public:
        ASTIfStatement(unsigned int charIndex) : IASTNode(charIndex, ASTNodeType::IfStatement) {}

        const std::shared_ptr<IASTNode>& GetExpression() const
        {
            return GetChild(0);
        }

        const std::shared_ptr<IASTNode>& GetBody() const
        {
            return GetChild(1);
        }

        const std::shared_ptr<IASTNode>& GetElseBody() const
        {
            return GetChild(2);
        }
    };

    class ASTWhileStatement : public IASTNode
    {
        public:
        ASTWhileStatement(unsigned int charIndex) : IASTNode(charIndex, ASTNodeType::WhileStatement) {}

        const std::shared_ptr<IASTNode>& GetExpression() const
        {
            return GetChild(0);
        }

        const std::shared_ptr<IASTNode>& GetBody() const
        {
            return GetChild(1);
        }
    };

    class ASTBlockStatement : public IASTNode
    {
        public:
        ASTBlockStatement(unsigned int charIndex) : IASTNode(charIndex, ASTNodeType::BlockStatement) {}
    };

    class ASTReturnStatement : public IASTNode
    {
        public:
        ASTReturnStatement(unsigned int charIndex) : IASTNode(charIndex, ASTNodeType::ReturnStatement) {}

        const std::shared_ptr<IASTNode>& GetExpression() const
        {
            return GetChild(0);
        }
    };

    class ASTFunctionDeclaration : public IASTNode
    {
        public:
        ASTFunctionDeclaration(const std::string& name, const std::vector<std::string>& args, unsigned int charIndex) :
            m_Name(name), m_Args(args), IASTNode(charIndex, ASTNodeType::FunctionDeclaration) {}

        const std::string& GetName() const
        {
            return m_Name;
        }

        size_t GetArgumentCount() const
        {
            return m_Args.size();
        }

        const std::vector<std::string>& GetArguments() const
        {
            return m_Args;
        }

        private:
        std::string m_Name;
        std::vector<std::string> m_Args;
    };

    class ASTTemplateFunction : public IASTNode
    {
        public:
        ASTTemplateFunction(const std::string& name, const std::vector<std::string>& args,
            const std::vector<std::string>& templateArgs, unsigned int charIndex) :
            m_Name(name), m_Args(args), m_TemplateArgs(templateArgs),
            IASTNode(charIndex, ASTNodeType::TemplateFunction) {}

        const std::string& GetName() const
        {
            return m_Name;
        }

        size_t GetArgumentCount() const
        {
            return m_Args.size();
        }

        const std::vector<std::string>& GetArguments() const
        {
            return m_Args;
        }

        size_t GetTemplateArgumentCount() const
        {
            return m_TemplateArgs.size();
        }

        const std::vector<std::string>& GetTemplateArguments() const
        {
            return m_TemplateArgs;
        }

        private:
        std::string m_Name;
        std::vector<std::string> m_Args;
        std::vector<std::string> m_TemplateArgs;
    };

    class ASTVariableDeclaration : public IASTNode
    {
        public:
        ASTVariableDeclaration(const std::string& name, unsigned int arraySize, unsigned int charIndex) :
            m_Name(name), m_ArraySize(arraySize), IASTNode(charIndex, ASTNodeType::VariableDeclaration) {}

        const std::string& GetName() const
        {
            return m_Name;
        }

        const std::shared_ptr<IASTNode>& GetExpression() const
        {
            return GetChild(0);
        }

        unsigned int GetArraySize() const
        {
            return m_ArraySize;
        }

        private:
        std::string m_Name;
        unsigned int m_ArraySize;
    };

    class ASTEventCondition : public IASTNode
    {
        public:
        ASTEventCondition(const std::string& name, unsigned int charIndex) :
            m_Name(name), IASTNode(charIndex, ASTNodeType::EventCondition) {}

        const std::string& GetName() const
        {
            return m_Name;
        }

        const std::shared_ptr<IASTNode>& GetArgument(int index) const
        {
            return GetChild(index);
        }

        private:
        std::string m_Name;
    };

    class ASTEventDeclaration : public IASTNode
    {
        public:
        ASTEventDeclaration(unsigned int charIndex) : IASTNode(charIndex, ASTNodeType::EventDeclaration) {}

        unsigned int GetConditionsCount() const
        {
            return GetChildCount() - 1;
        }

        const std::shared_ptr<IASTNode>& GetCondition(int index) const
        {
            if (index >= (int)GetChildCount() - 1)
            {
                return GetChild(-1); // intentional, will return null
            }

            return GetChild(index);
        }

        const std::shared_ptr<IASTNode>& GetBody() const
        {
            return GetChild(GetChildCount() - 1);
        }
    };

    class ASTRepeatTemplate : public IASTNode
    {
        public:
        ASTRepeatTemplate(const std::string& iteratorName, unsigned int charIndex) :
            m_IteratorName(iteratorName), IASTNode(charIndex, ASTNodeType::RepeatTemplate)
        {}

        const std::string& GetIteratorName() const
        {
            return m_IteratorName;
        }

        const std::vector<std::shared_ptr<IASTNode>>& GetList() const
        {
            return m_List;
        }

        void AddListItem(const std::shared_ptr<IASTNode>& node)
        {
            m_List.push_back(node);
        }

        private:
        std::string m_IteratorName;
        std::vector<std::shared_ptr<IASTNode>> m_List;
    };

    class ASTUnitProperties : public IASTNode
    {
        public:
        ASTUnitProperties(const std::string& name, unsigned int charIndex) :
            m_Name(name), IASTNode(charIndex, ASTNodeType::UnitProperties) {}

        unsigned int GetPropertiesCount() const
        {
            return GetChildCount();
        }

        const std::shared_ptr<IASTNode>& GetProperty(int index) const
        {
            return GetChild(index);
        }

        const std::string& GetName() const
        {
            return m_Name;
        }

        private:
        std::string m_Name;
    };

    class ASTUnitProperty : public IASTNode
    {
        public:
        ASTUnitProperty(const std::string& name, unsigned int value, unsigned int charIndex) :
            m_Name(name), m_Value(value), IASTNode(charIndex, ASTNodeType::UnitProperty) {}

        const std::string& GetName() const
        {
            return m_Name;
        }

        unsigned int GetValue() const
        {
            return m_Value;
        }

        private:
        std::string m_Name;
        unsigned int m_Value;
    };

}

#endif
