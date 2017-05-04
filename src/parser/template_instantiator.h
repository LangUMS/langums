#ifndef __LANGUMS_TEMPLATEINSTANTIATOR_H
#define __LANGUMS_TEMPLATEINSTANTIATOR_H

#include <memory>
#include <unordered_map>

#include "../ast/ast.h"

namespace Langums
{

    class TemplateInstantiatorException : public std::exception
    {
        public:
        TemplateInstantiatorException(const char* error, IASTNode* node) : m_Node(node), std::exception(error) {}
        TemplateInstantiatorException(const std::string& error, IASTNode* node) : m_Node(node), std::exception(error.c_str()) {}


        IASTNode* GetASTNode() const
        {
            return m_Node;
        }

        private:
        IASTNode* m_Node = nullptr;
    };

    class TemplateInstantiator
    {
        public:
        void Process(const std::unique_ptr<IASTNode>& unit);

        private:
        void InstantiateRepeatTemplates(IASTNode* node);
        void InstantiateTemplateFunctions(IASTNode* node);
        void InstantiateTemplateFunction(ASTTemplateFunction* templateFunction, ASTFunctionCall* functionCall);
        std::vector<std::unique_ptr<IASTNode>> InstantiateRepeatTemplate(ASTRepeatTemplate* repeatTemplate);

        void ReplaceIdentifier(IASTNode* node, const std::string& identifier, const std::shared_ptr<IASTNode>& replaceWith);
        
        std::unique_ptr<IASTNode> CloneNode(IASTNode* node);

        std::unordered_map<std::string, ASTTemplateFunction*> m_TemplateFunctions;
        std::unordered_map<std::string, ASTFunctionDeclaration*> m_InstantiatedTemplates;

        IASTNode* m_Unit = nullptr;
    };

}

#endif
