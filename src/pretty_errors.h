#ifndef __LANGUMS_PRETTY_ERRORS_H
#define __LANGUMS_PRETTY_ERRORS_H

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

namespace LangUMS
{

    void PrintCodeContext(const std::string& src, unsigned int charIndex);
    void PrintPreprocessorException(const std::string& src, const PreprocessorException& ex);
    void PrintParserException(const std::string& src, const ParserException& ex);
    void PrintTemplateInstantiatorException(const std::string& src, const TemplateInstantiatorException& ex);
    void PrintIRCompilerException(const std::string& src, const IRCompilerException& ex);
    void PrintCompilerException(const std::string& src, const CompilerException& ex);

}

#endif
