#ifndef __LANGUMS_IR_EXCEPTIONS_H
#define __LANGUMS_IR_EXCEPTIONS_H

namespace LangUMS
{

    class IRCompilerException : public std::exception
    {
        public:
        IRCompilerException(const char* error, IASTNode* node) : m_Node(node), std::exception(error) {}

        IRCompilerException(const std::string& error, IASTNode* node) : m_Node(node), std::exception(error.c_str()) {}

        IASTNode* GetASTNode() const
        {
            return m_Node;
        }

        private:
        IASTNode* m_Node;
    };

}

#endif
