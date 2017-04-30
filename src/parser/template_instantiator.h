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
        TemplateInstantiatorException(const char* error) : std::exception(error) {}
        TemplateInstantiatorException(const std::string& error) : std::exception(error.c_str()) {}
    };

    class TemplateInstantiator
    {
        public:
        void Process(const std::unique_ptr<IASTNode>& unit);

        private:
        void ProcessInternal(IASTNode* node);
        void InstantiateTemplates(IASTNode* node);
        void InstantiateTemplateFunction(ASTTemplateFunction* templateFunction, ASTFunctionCall* functionCall);
        void ReplaceIdentifier(IASTNode* node, const std::string& identifier, const std::shared_ptr<IASTNode>& replaceWith);
        
        std::unique_ptr<IASTNode> CloneNode(IASTNode* node);

        std::unordered_map<std::string, ASTTemplateFunction*> m_TemplateFunctions;
        std::unordered_map<std::string, ASTFunctionDeclaration*> m_InstantiatedTemplates;

        IASTNode* m_Unit = nullptr;
    };

}

#endif
