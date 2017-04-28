#include <iostream>
#include <experimental/filesystem>

#include "log.h"
#include "cxxopts.h"
#include "stringutil.h"
#include "libmpq/SFmpqapi.h"
#include "libchk/chk.h"
#include "parser/preprocessor.h"
#include "parser/parser.h"
#include "parser/template_instantiator.h"
#include "parser/ast_optimizer.h"
#include "compiler/ir.h"
#include "compiler/compiler.h"
#include "compiler/registermap_parser.h"

#undef min
#undef max

using namespace Langums;
using namespace CHK;
using namespace std::experimental;

#define SCENARIO_FILENAME "staredit\\scenario.chk"

int main(int argc, char* argv[])
{
    cxxopts::Options opts("LangUMS", "LangUMS compiler");

    opts.add_options()
        ("h,help", "Prints this help message", cxxopts::value<bool>())
        ("s,src", "Source .scx map file", cxxopts::value<std::string>())
        ("d,dst", "Destination .scx map file", cxxopts::value<std::string>())
        ("l,lang", "Source .l script file", cxxopts::value<std::string>())
        ("r,reg", "Available registers map file", cxxopts::value<std::string>())
        ("strip", "Strips unnecessary data from the resulting .scx. Will make the file unopenable in editors.", cxxopts::value<bool>())
        ("preserve-triggers", "Preserves already existing triggers in the map. (Use with caution!)", cxxopts::value<bool>())
        ("copy-batch-size", "Maximum number value that can be copied in one cycle. Must be power of 2. Higher values will increase the amount of emitted triggers. (default: 65536)", cxxopts::value<unsigned int>())
        ("triggers-owner", "The index of the player which holds the main logic triggers (default: 1)", cxxopts::value<unsigned int>())
        ("disable-optimization", "Disables all forms of compiler optimization (useful to debug compiler issues)", cxxopts::value<bool>())
        ;
    opts.parse(argc, argv);

    if (opts.count("help") > 0)
    {
        LOG_F("%", opts.help());
        Log::Log::Instance()->Destroy();
        return 0;
    }

    if (opts.count("src") != 1)
    {
        LOG_F("%", opts.help());
        LOG_EXITERR("\n(!) Arguments must contain exactly one --src file");
        return 1;
    }

    if (opts.count("dst") != 1)
    {
        LOG_F("%", opts.help());
        LOG_EXITERR("\n(!) Arguments must contain exactly one --dst file");
        return 1;
    }

    if (opts.count("lang") != 1)
    {
        LOG_F("%", opts.help());
        LOG_EXITERR("\n(!) Arguments must contain exactly one --lang file");
        return 1;
    }

    auto langPath = filesystem::path(opts["lang"].as<std::string>());
    if (!langPath.has_filename())
    {
        LOG_F("%", opts.help());
        LOG_EXITERR("\n(!) --lang path must be a valid .l file.");
        return 1;
    }

    auto srcPath = filesystem::path(opts["src"].as<std::string>());
    if (!srcPath.has_filename())
    {
        LOG_F("%", opts.help());
        LOG_EXITERR("\n(!) --src path must be a valid .scx file.");
        return 1;
    }

    auto dstPath = filesystem::path(opts["dst"].as<std::string>());

    if (filesystem::is_directory(dstPath))
    {
        dstPath += srcPath.filename();
    }

    LOG_F("Source map: %", srcPath);
    LOG_F("Code source: %", langPath);
    LOG_F("Destination map: %", dstPath);

    auto filename = srcPath.filename();

    auto tempPath = filesystem::temp_directory_path();
    tempPath += "langums_";
    tempPath += filename.c_str();
    std::error_code ec;

    if (!filesystem::copy_file(srcPath, tempPath, filesystem::copy_options::overwrite_existing, ec))
    {
        LOG_EXITERR("\n(!) Failed to create temporary file, error code: %", ec);
        return 1;
    }

    MPQHANDLE mpq = nullptr;
    auto mpqPath = tempPath.generic_u8string();
    if (!SFileOpenArchive(mpqPath.c_str(), 0, SFILE_OPEN_ALLOW_WRITE, &mpq))
    {
        auto errorCode = GetLastError();
        LOG_EXITERR("\n(!) SFileOpenArchive failed, error code: %. Looks like an invalid scx file.", errorCode);
        return 1;
    }

    LOG_F("Looks like a valid MPQ file.");
     
    MPQHANDLE scenarioFile = nullptr;
    if (!SFileOpenFileEx(mpq, SCENARIO_FILENAME, 0, &scenarioFile))
    {
        LOG_EXITERR("\n(!) Failed to find scenario.chk inside MPQ, not a Brood War map file?");
        return 1;
    }

    auto scenarioFileSize = SFileGetFileSize(scenarioFile, 0);

    std::vector<char> scenarioBytes;
    scenarioBytes.resize(scenarioFileSize);

    if (!SFileReadFile(scenarioFile, scenarioBytes.data(), scenarioFileSize, 0, 0))
    {
        LOG_EXITERR("\n(!) SFileReadFile failed for scenario.chk.");
        return 1;
    }

    LOG_F("Read scenario.chk (% bytes).", scenarioFileSize);

    auto stripExtraData = opts["strip"].count() > 0;
    CHK::File chk(scenarioBytes, stripExtraData);
    if (!SFileCloseFile(scenarioFile))
    {
        LOG_EXITERR("\n(!) Failed to close scenario.chk file.");
        return 1;
    }

    scenarioFile = nullptr;

    if (!chk.HasChunk("VER "))
    {
        LOG_EXITERR("\n(!) VER chunk not found in scenario.chk. Map file is corrupted.");
        return 1;
    }

    auto& versionChunk = chk.GetFirstChunk<CHKVerChunk>("VER ");
    auto version = versionChunk.GetVersion();
    if (version != CHK_LATEST_MAP_VERSION)
    {
        LOG_EXITERR("\n(!) Fatal error! LangUMS is not compatible with older maps. Map version is %, expected %.", version, CHK_LATEST_MAP_VERSION);
        return 1;
    }

    if (!chk.HasChunk("TRIG"))
    {
        LOG_EXITERR("\n(!) TRIG chunk not found in scenario.chk. Add at least one trigger before running LangUMS.");
        return 1;
    }

    if (!chk.HasChunk("DIM "))
    {
        LOG_EXITERR("\n(!) DIM chunk not found in scenario.chk. Map file is corrupted.");
        return 1;
    }

    /*auto& ownrChunk = chk.GetFirstChunk<CHKOwnrChunk>("OWNR");
    if (ownrChunk.GetPlayerType(7) != PlayerType::Computer)
    {
        LOG_F("(!) Warning! Player 8 is not set to type \"Computer\". Overriding.");
        ownrChunk.SetPlayerType(7, PlayerType::Computer);
    }*/

    auto& triggersChunk = chk.GetFirstChunk<CHKTriggersChunk>("TRIG");
    triggersChunk = chk.GetFirstChunk<CHKTriggersChunk>("TRIG");

//    auto trigger1 = triggersChunk.GetTrigger(0);

    auto preserveTriggers = opts.count("preserve-triggers") > 0;

    LOG_F("Pre-existing trigger count: % (%)", triggersChunk.GetTriggersCount(), preserveTriggers ? "preserved" : "not preserved");

    auto& dimChunk = chk.GetFirstChunk<CHKDimChunk>("DIM ");
    LOG_F("Map size is %x%", dimChunk.GetWidth(), dimChunk.GetHeight());
    
    LOG_F("");

    LOG_F("Compiling script \"%\"", langPath.generic_u8string());

    std::ifstream scriptFile(langPath.generic_u8string());
    if (!scriptFile.is_open())
    {
        LOG_EXITERR("\n(!) Failed to open \"%\" for reading.", langPath.generic_u8string());
        return 1;
    }

    std::string str;
    std::string script;
    while (std::getline(scriptFile, str))
    {
        script += str;
        script.push_back('\n');
    }

    script.push_back('\n');
    
    Preprocessor preprocessor(langPath.relative_path().remove_filename().generic_u8string());

    try
    {
        script = preprocessor.Process(script);
    }
    catch (const PreprocessorException& ex)
    {
        LOG_EXITERR("Preprocessor error: %", ex.what());
        return 1;
    } 

    auto disableOptimization = opts.count("disable-optimization") > 0;

    Parser parser;
    std::shared_ptr<IASTNode> ast;
    
    ASTOptimizer astOptimizer;

    try
    {
        ast = std::shared_ptr<IASTNode>(parser.Parse(script));

        if (!disableOptimization)
        {
            ast = astOptimizer.Process(ast);
        }
    }
    catch (const TemplateInstantiatorException& ex)
    {
        LOG_EXITERR("Template instantation error - %", ex.what());
        return -1;
    }
    catch (const ParserException& ex)
    {
        auto charPos = ex.GetCharPosition();
        auto& src = parser.GetSourceBuffer();

        auto context = src.substr(std::max(charPos - 16, 0), 32);

        auto newLines = 0;
        for (auto i = 0; i < std::min((int)src.length(), charPos); i++)
        {
            if (src[i] == '\n')
            {
                newLines++;
            }
        }

        std::istringstream iss(src);
        std::string output;

        std::vector<std::string> lines;

        for (std::string line; std::getline(iss, line); )
        {
            auto ln = trim(line);
            if (ln.length() > 0)
            {
                lines.push_back(ln);
            }
        }

        auto sub = src.substr(charPos);
        auto l = sub.find_first_of('\n');

        sub = sub.substr(0, l);
        LOG_F("\n(!) Parser error: % on line %", ex.what(), newLines + 2);
        LOG_F("Near code \"%\"\n", sub);
        
        for (auto i = std::max(0, newLines - 1); i < std::min((int)lines.size(), newLines + 2); i++)
        {
            LOG_F(">>> %", lines[i]);
        }

        LOG_EXITERR("");
        return 1;
    }

    IRCompiler ir;
    
    try
    {
        ir.Compile(ast);
    }
    catch (const IRCompilerException& ex)
    {
        LOG_EXITERR("\n(!) IR Compiler Error: %", ex.what());
        return 1;
    }

    try
    {
        if (!disableOptimization)
        {
            ir.Optimize();
        }
    }
    catch (const IRCompilerException& ex)
    {
        LOG_EXITERR("\n(!) IR Optimizer Error: %", ex.what());
        return 1;
    }

    LOG_F("\n----------- IR Dump -----------");
    LOG_F("%", ir.DumpInstructions(true));
    LOG_F("-------------------------------\n");

    auto& instructions = ir.GetInstructions();

    LOG_F("Emitted % instructions.", instructions.size());

    Compiler compiler;

    if (opts.count("triggers-owner") > 0)
    {
        auto triggerOwner = opts["triggers-owner"].as<unsigned int>();
        if (triggerOwner < 1 || triggerOwner > 8)
        {
            LOG_EXITERR("\n(!) triggers-owner must be between 1 and 8");
            return 1;
        }

        LOG_F("Main trigger owner: %", triggerOwner);
        compiler.SetTriggersOwner(triggerOwner);
    }

    if (opts.count("copy-batch-size") > 0)
    {
        auto copyBatchSize = opts["copy-batch-size"].as<unsigned int>();

        if ((copyBatchSize & (copyBatchSize - 1)) != 0)
        {
            LOG_EXITERR("\n(!) copy-batch-size must be a power of 2!");
            return 1;
        }

        LOG_F("Copy batch size: %", copyBatchSize);

        if (copyBatchSize < 1024)
        {
            LOG_F("(!) WARNING! Copy batch size is set to an extremely low value (< 1024). Arithmetic operations with large numbers will be VERY slow to execute.");
        }

        compiler.SetCopyBatchSize(copyBatchSize);
    }

    if (opts.count("reg") > 0)
    {
        auto regPath = filesystem::path(opts["reg"].as<std::string>());
        if (!regPath.has_filename())
        {
            LOG_F("%", opts.help());
            LOG_EXITERR("\n(!) --reg path must be a valid text file.");
            return 1;
        }

        std::ifstream regFile(regPath.generic_u8string());
        if (!regFile.is_open())
        {
            LOG_EXITERR("\n(!) Failed to open \"%\" for reading.", regPath.generic_u8string());
            return 1;
        }

        LOG_F("Using registers map from \"%\"", regPath.generic_u8string());

        std::string str;
        std::string regFileContents;
        while (std::getline(regFile, str))
        {
            regFileContents += str;
            regFileContents.push_back('\n');
        }

        RegistermapParser registerParser;

        std::vector<RegisterDef> defs;

        try
        {
            defs = registerParser.Parse(regFileContents);
        }
        catch (RegistermapParserException& ex)
        {
            LOG_EXITERR("\n(!) Register map parser error: %", ex.what());
            return 1;
        }

        if (defs.size() < 24)
        {
            LOG_F("(!) Warning! Registers map contains less than 24 items. There is a high chance of stack overflow.");
        }

        compiler.SetCustomRegisterDefinitions(defs);
    }

    try
    {
        compiler.Compile(instructions, chk, preserveTriggers);
    }
    catch (const CompilerException& ex)
    {
        LOG_EXITERR("\n(!) CodeGen error: %", ex.what());
        return 1;
    }

    LOG_F("Compilation successful! Trigger count: %", triggersChunk.GetTriggersCount());

    std::vector<char> chkBytes;
    chk.Serialize(chkBytes);

    if (!MpqAddFileFromBuffer(mpq, chkBytes.data(), chkBytes.size(), SCENARIO_FILENAME, MAFA_REPLACE_EXISTING))
    {
        LOG_EXITERR("Failed to write out scenario.chk to MPQ archive.");
        return 1;
    }

    SFileCloseArchive(mpq);

    auto fileSize = std::experimental::filesystem::file_size(tempPath);
    LOG_F("Final size: % kb", fileSize / 1024);

    if (!filesystem::copy_file(tempPath, dstPath, filesystem::copy_options::overwrite_existing, ec))
    {
        LOG_EXITERR("Failed to create destination file, error code: %", ec);
        return 1;
    }

    LOG_F("Written to: %", dstPath.generic_u8string());

    LOG_DEINIT();
    return 0;
}
