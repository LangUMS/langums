#include <string>
#include <algorithm>
#include <vector>

#include "log.h"
#include "stringutil.h"

#include "parser/preprocessor.h"
#include "parser/parser.h"
#include "parser/template_instantiator.h"

#include "compiler/ir.h"
#include "compiler/compiler.h"

#include "pretty_errors.h"

namespace LangUMS
{

    void PrintCodeContext(const std::string& src, unsigned int charIndex)
    {
        auto lines = Split(src);

        auto lineNumber = GetLineNumber(src, charIndex);
        auto startLine = std::max(lineNumber - 2, 0u);
        auto endLine = std::min(lineNumber + 1, lines.size() - 1);

        LOG_F("\nNear code:");

        for (auto i = startLine; i < endLine; i++)
        {
            LOG_F(">>> %", lines[i]);
        }
    }

    void PrintPreprocessorException(const std::string& src, const PreprocessorException& ex)
    {
        LOG_F("\n(!) Preprocessor error: %", ex.what());
    }

    void PrintParserException(const std::string& src, const ParserException& ex)
    {
        auto charIndex = ex.GetCharPosition();
        auto lineNumber = GetLineNumber(src, charIndex);

        LOG_F("\n(!) Parser error: % on line %", ex.what(), lineNumber);
        PrintCodeContext(src, charIndex);
    }

    void PrintTemplateInstantiatorException(const std::string& src, const TemplateInstantiatorException& ex)
    {
        auto astNode = ex.GetASTNode();
        auto astNodeType = astNode->GetTypeName();

        auto charIndex = astNode->GetCharIndex();
        auto lineNumber = GetLineNumber(src, charIndex);

        LOG_F("\n(!) Template instantiation error: % in % on line %", ex.what(), astNodeType, lineNumber);
        PrintCodeContext(src, charIndex);
    }

    void PrintIRCompilerException(const std::string& src, const IRCompilerException& ex)
    {
        auto astNode = ex.GetASTNode();

        if (astNode != nullptr)
        {
            auto astNodeType = astNode->GetTypeName();

            auto charIndex = astNode->GetCharIndex();
            auto lineNumber = GetLineNumber(src, charIndex);

            auto fn = FindFunctionDeclarationForNode(astNode);
            if (fn != nullptr)
            {
                LOG_F("\n(!) Compilation error: % in % on line % in function \"%\"", ex.what(), astNodeType, lineNumber, fn->GetName());
            }
            else
            {
                LOG_F("\n(!) Compilation error: % in % on line %", ex.what(), astNodeType, lineNumber);
            }

            PrintCodeContext(src, charIndex);
        }
        else
        {
            LOG_F("\n(!) Compilation error: %", ex.what());
        }
    }

    void PrintCompilerException(const std::string& src, const CompilerException& ex)
    {
        auto instruction = ex.GetInstruction();
        if (!instruction)
        {
            LOG_F("\n(!) Codegen error: %", ex.what());
            return;
        }

        auto astNode = instruction->GetASTNode();
        auto astNodeType = astNode->GetTypeName();

        auto charIndex = astNode->GetCharIndex();
        auto lineNumber = GetLineNumber(src, charIndex);

        auto fn = FindFunctionDeclarationForNode(astNode);
        if (fn != nullptr)
        {
            LOG_F("\n(!) Codegen error: % in % on line % in function \"%\"", ex.what(), astNodeType, lineNumber, fn->GetName());
        }
        else
        {
            LOG_F("\n(!) Codegen error: % in % on line %", ex.what(), astNodeType, lineNumber);
        }

        LOG_F("\n(!) Codegen error: % in % on line %", ex.what(), astNodeType, lineNumber);
        PrintCodeContext(src, charIndex);
    }

}
