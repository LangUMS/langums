#ifndef __LANGUMS_PRETTY_ERRORS_H
#define __LANGUMS_PRETTY_ERRORS_H

#include <string>
#include <algorithm>
#include <vector>

#undef min
#undef max

#include "log.h"
#include "parser/preprocessor.h"
#include "parser/parser.h"
#include "parser/template_instantiator.h"
#include "compiler/ir.h"
#include "compiler/compiler.h"

namespace LangUMS
{

    inline std::vector<std::string> Split(const std::string& src)
    {
        std::stringstream ss(src);
        std::string line;
        
        std::vector<std::string> lines;

        while (std::getline(ss, line, '\n'))
        {
            lines.push_back(line);
        }

        return lines;
    }

    inline unsigned int GetLineNumber(const std::string& src, int charIndex)
    {
        auto lineNumber = 1;
        for (auto i = 0; i < std::min((int)src.length(), charIndex); i++)
        {
            if (src[i] == '\n')
            {
                lineNumber++;
            }
        }

        return lineNumber;
    }

    inline void PrintCodeContext(const std::string& src, unsigned int charIndex)
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

    inline void PrintPreprocessorException(const std::string& src, const PreprocessorException& ex)
    {
        LOG_F("\n(!) Preprocessor error: %", ex.what());
    }

    inline void PrintParserError(const std::string& src, const ParserException& ex)
    {
        auto charIndex = ex.GetCharPosition();
        auto lineNumber = GetLineNumber(src, charIndex);

        LOG_F("\n(!) Parser error: % on line %", ex.what(), lineNumber);
        PrintCodeContext(src, charIndex);
    }

    inline void PrintTemplateInstantiatorException(const std::string& src, const TemplateInstantiatorException& ex)
    {
        auto astNode = ex.GetASTNode();
        auto astNodeType = astNode->GetTypeName();

        auto charIndex = astNode->GetCharIndex();
        auto lineNumber = GetLineNumber(src, charIndex);

        LOG_F("\n(!) Template instantiation error: % in type \"%\" on line %", ex.what(), astNodeType, lineNumber);
        PrintCodeContext(src, charIndex);
    }

    inline void PrintIRCompilerException(const std::string& src, const IRCompilerException& ex)
    {
        auto astNode = ex.GetASTNode();

        if (astNode != nullptr)
        {
            auto astNodeType = astNode->GetTypeName();

            auto charIndex = astNode->GetCharIndex();
            auto lineNumber = GetLineNumber(src, charIndex);

            LOG_F("\n(!) Compilation error: % in type \"%\" on line %", ex.what(), astNodeType, lineNumber);
            PrintCodeContext(src, charIndex);
        }
        else
        {
            LOG_F("\n(!) Compilation error: %", ex.what());
        }
    }

    inline void PrintCompilerException(const std::string& src, const CompilerException& ex)
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

        LOG_F("\n(!) Codegen error: % in type \"%\" on line %", ex.what(), astNodeType, lineNumber);
        PrintCodeContext(src, charIndex);
    }

}

#endif
