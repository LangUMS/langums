#include <iostream>
#include <fstream>
#include <experimental/filesystem>

#include "log.h"
#include "log_interface_stdout.h"

#include "cxxopts.h"
#include "stringutil.h"
#include "libchk/src/chk.h"
#include "parser/preprocessor.h"
#include "parser/parser.h"
#include "parser/template_instantiator.h"
#include "parser/ast_optimizer.h"
#include "compiler/ir.h"
#include "compiler/compiler.h"
#include "compiler/registermap_parser.h"
#include "wavinfo.h"
#include "pretty_errors.h"
#include "mpq_wrapper.h"
#include "debugger/debugger.h"
#include "debugger/debugger_cli.h"
#include "debugger/debugger_vscode.h"
#include "debugger/log_interface_vscode.h"

#undef min
#undef max

#define VERSION "v0.1.5"

using namespace Langums;
using namespace CHK;
using namespace std::experimental;

#define SCENARIO_FILENAME "staredit\\scenario.chk"
#define DEFAULT_PROCESS_NAME "starcraft.exe"

int main(int argc, char* argv[])
{
    cxxopts::Options opts("LangUMS " VERSION, "LangUMS compiler");

    opts.add_options()
        ("h,help", "Prints this help message.", cxxopts::value<bool>())
        ("v,version", "Prints the current version of the compiler.", cxxopts::value<bool>())
        ("s,src", "Path to source .scx map file.", cxxopts::value<std::string>())
        ("d,dst", "Path to destination .scx map file.", cxxopts::value<std::string>())
        ("l,lang", "Path to source .l source file.", cxxopts::value<std::string>())
        ("r,reg", "Optional registers file.", cxxopts::value<std::string>())
        ("strip", "Strips unnecessary data from the resulting .scx. Will make the file unopenable in editors.", cxxopts::value<bool>())
        ("preserve-triggers", "Preserves already existing triggers in the map (use with caution!).", cxxopts::value<bool>())
        ("copy-batch-size", "Maximum number value that can be copied in one cycle. Must be a power of 2. Higher values will increase the amount of emitted triggers (default: 8192).", cxxopts::value<unsigned int>())
        ("triggers-owner", "The index of the player which holds the main logic triggers (default: 1).", cxxopts::value<unsigned int>())
        ("disable-optimization", "Disables all forms of compiler optimization (useful to debug compiler issues).", cxxopts::value<bool>())
        ("disable-compression", "Disables compression of the resulting map file. Results in much larger file sizes but you can open the map in StarEdit.", cxxopts::value<bool>())
        ("dump-ir", "Dumps the intermediate representation during compilation.", cxxopts::value<bool>())
        ("force", "Forces the compiler to do thing it shouldn't.", cxxopts::value<bool>())
        ("debug", "Attaches the debugger after compiling the map. (experimental!)", cxxopts::value<bool>())
        ("debug-process", "Sets the StarCraft process name for debugging (default: starcraft.exe).", cxxopts::value<std::string>())
        ("debug-vscode", "Internal. Used by the VS Code debugger extension to communicate with LangUMS.", cxxopts::value<bool>())
        ;

    try
    {
        opts.parse(argc, argv);
    }
    catch (...)
    {
        LOG_F("Invalid arguments.", opts.help());

        LOG_F("%", opts.help());
        Log::Instance()->Destroy();
        return 1;
    }

    if (opts.count("help") > 0)
    {
        LOG_F("%", opts.help());
        Log::Instance()->Destroy();
        return 0;
    }

    if (opts.count("version") > 0)
    {
        std::cout << "LangUMS " << VERSION << std::endl;
        return 0;
    }

    std::unique_ptr<DebuggerVSCode> vsCodeDebugger;

    if (opts.count("debug-vscode") > 0)
    {
        vsCodeDebugger = std::make_unique<DebuggerVSCode>();
        auto logInterface = new LogInterfaceVSCode();
        Log::Instance()->AddInterface(std::unique_ptr<ILogInterface>(logInterface));
        vsCodeDebugger->SetLogInterface(logInterface);
    }
    else
    {
        Log::Instance()->AddInterface(std::unique_ptr<ILogInterface>(new LogInterfaceStdout()));
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
        LOG_EXITERR("\n(!) --lang path must be a valid LangUMS source file.");
        return 1;
    }

    if (!filesystem::exists(langPath))
    {
        LOG_EXITERR("\n(!) Fatal error, source file \"%\" does not exist. Bailing out.", langPath.generic_u8string());
        return 1;
    }

    LOG_F("Code source: %", langPath);

    std::ifstream sourceFile(langPath.generic_u8string());
    if (!sourceFile.is_open())
    {
        LOG_EXITERR("\n(!) Failed to open \"%\" for reading.", langPath.generic_u8string());
        return 1;
    }

    std::string str;
    std::string source;
    while (std::getline(sourceFile, str))
    {
        source += str;
        source.push_back('\n');
    }

    source.push_back('\n');

    Preprocessor preprocessor(langPath.relative_path().remove_filename().generic_u8string());

    try
    {
        source = preprocessor.Process(source);
    }
    catch (const PreprocessorException& ex)
    {
        PrintPreprocessorException(source, ex);
        LOG_EXITERR("");
        return 1;
    }

    auto hasSrcPath = false;
    filesystem::path srcPath;

    auto hasDstPath = false;
    filesystem::path dstPath;

    if (preprocessor.HasMapName())
    {
        srcPath = preprocessor.GetMapName();
        if (!srcPath.is_absolute())
        {
            auto langRoot = langPath;
            langRoot.remove_filename();
            if (filesystem::is_regular_file(langRoot))
            {
                srcPath = "./";
            }
            else
            {
                srcPath = langRoot;
            }
            
            srcPath.append(preprocessor.GetMapName());
        }

        hasSrcPath = true;
        LOG_F("Map file specified with #src in source code.");
    }

    if (preprocessor.HasOutMapName())
    {
        dstPath = preprocessor.GetOutMapName();

        if (!dstPath.is_absolute())
        {
            auto langRoot = langPath;
            langRoot.remove_filename();
            dstPath = langRoot;
            dstPath.append(preprocessor.GetOutMapName());
        }

        hasDstPath = true;
        LOG_F("Destination map file specified with #dst in source code.");
    }

    if (!hasSrcPath)
    {
        if (opts.count("src") != 1)
        {
            LOG_F("%", opts.help());
            LOG_EXITERR("\n(!) Arguments must contain exactly one --src file");
            return 1;
        }

        srcPath = filesystem::path(opts["src"].as<std::string>());
        if (!srcPath.has_filename())
        {
            LOG_F("%", opts.help());
            LOG_EXITERR("\n(!) --src path must be a valid .scx file.");
            return 1;
        }
    }

    if (!hasDstPath)
    {
        if (opts.count("dst") != 1)
        {
            LOG_F("%", opts.help());
            LOG_EXITERR("\n(!) Arguments must contain exactly one --dst file");
            return 1;
        }

        dstPath = filesystem::path(opts["dst"].as<std::string>());

        if (srcPath == dstPath)
        {
            LOG_EXITERR("\n(!) Fatal error, source and destination path are the same. Bailing out.");
            return 1;
        }
    }

    if (!filesystem::exists(srcPath))
    {
        LOG_EXITERR("\n(!) Fatal error, source map file \"%\" does not exist. Bailing out.", srcPath.generic_u8string());
        return 1;
    }

    if (filesystem::is_directory(dstPath))
    {
        dstPath += srcPath.filename();
    }

    LOG_F("Source map: %", srcPath);
    LOG_F("Destination map: %", dstPath);
    LOG_F("");

    auto filename = srcPath.filename();
    
    srand((unsigned int)time(nullptr));

    auto tempPath = filesystem::temp_directory_path();
    tempPath += "langums_";
    tempPath += SafePrintf("%_", rand());
    tempPath += filename.c_str();

    std::error_code ec;

    if (!filesystem::copy_file(srcPath, tempPath, filesystem::copy_options::overwrite_existing, ec))
    {
        LOG_EXITERR("\n(!) Failed to create temporary file, error code: %", ec);
        return 1;
    }

    std::unique_ptr<MPQWrapper> mpqWrapper;

    try
    {
        mpqWrapper = std::make_unique<MPQWrapper>(tempPath.generic_u8string(), false);
    }
    catch (MPQWrapperException& ex)
    {
        LOG_EXITERR("\n(!) StormLib error: %", ex.what());
        return 1;
    }

    LOG_F("Map looks like a valid MPQ file.");
     
    std::vector<char> scenarioBytes;

    try
    {
        mpqWrapper->ReadFile(SCENARIO_FILENAME, scenarioBytes);
    }
    catch (MPQWrapperException& ex)
    {
        LOG_EXITERR("\n(!) StormLib error: %", ex.what());
        return 1;
    }

    LOG_F("Reading from scenario.chk (% bytes).", scenarioBytes.size());
    CHK::File chk(scenarioBytes, opts["strip"].count() > 0);

    auto& chunkTypes = chk.GetChunkTypes();
    std::string chunkTypesString;

    auto i = 0u;
    for (auto& type : chunkTypes)
    {
        chunkTypesString.append(trim(type));

        if (i != chunkTypes.size() - 1)
        {
            chunkTypesString.append(", ");
        }

        i++;
    }

    LOG_F("Discovered chunks: %", chunkTypesString);

    if (!chk.HasChunk(ChunkType::VER))
    {
        LOG_EXITERR("\n(!) VER chunk not found in scenario.chk. Map file is corrupted.");
        return 1;
    }

    auto versionChunk = chk.GetFirstChunk<VERChunk>(ChunkType::VER);
    auto version = versionChunk->GetVersion();
    if (version != CHK_LATEST_MAP_VERSION)
    {
        LOG_EXITERR("\n(!) Fatal error! LangUMS is not compatible with older maps. Map version is %, expected %.", version, CHK_LATEST_MAP_VERSION);
        return 1;
    }

    if (!chk.HasChunk(ChunkType::TRIG))
    {
        std::vector<char> bytes;
        chk.AddChunk(std::unique_ptr<IChunk>(new TRIGChunk(bytes, "TRIG")));
    }

    if (!chk.HasChunk(ChunkType::DIM))
    {
        LOG_EXITERR("\n(!) DIM chunk not found in scenario.chk. Map file is corrupted.");
        return 1;
    }

    auto triggersChunk = chk.GetFirstChunk<TRIGChunk>(ChunkType::TRIG);
    auto stringsChunk = chk.GetFirstChunk<STRChunk>(ChunkType::STR);

    auto preserveTriggers = opts.count("preserve-triggers") > 0;

    LOG_F("Existing trigger count: % (%)", triggersChunk->GetTriggersCount(), preserveTriggers ? "preserved" : "not preserved");

    auto dimChunk = chk.GetFirstChunk<DIMChunk>(ChunkType::DIM);
    LOG_F("Map size: %x%", dimChunk->GetWidth(), dimChunk->GetHeight());
    
    auto tilesetChunk = chk.GetFirstChunk<ERAChunk>(ChunkType::ERA);
    LOG_F("Tileset: %", tilesetChunk->GetTilesetTypeString());

    auto ownrChunk = chk.GetFirstChunk<OWNRChunk>(ChunkType::OWNR);

    LOG_F("");

    LOG_F("Compiling source \"%\"", langPath.generic_u8string());

    auto disableOptimization = opts.count("disable-optimization") > 0;

    Parser parser;
    std::shared_ptr<IASTNode> ast;
    
    ASTOptimizer astOptimizer;

    try
    {
        ast = std::shared_ptr<IASTNode>(parser.Parse(source));

        if (!disableOptimization)
        {
            ast = astOptimizer.Process(ast);
        }
    }
    catch (const TemplateInstantiatorException& ex)
    {
        PrintTemplateInstantiatorException(parser.GetSourceBuffer(), ex);
        LOG_EXITERR("");
        return -1;
    }
    catch (const ParserException& ex)
    {
        PrintParserError(parser.GetSourceBuffer(), ex);
        LOG_EXITERR("");
        return 1;
    }

    source = parser.GetSourceBuffer();

    IRCompiler ir;
    
    try
    {
        ir.Compile(ast);
    }
    catch (const IRCompilerException& ex)
    {
        PrintIRCompilerException(source, ex);
        LOG_EXITERR("");
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
        PrintIRCompilerException(source, ex);
        LOG_EXITERR("");
        return 1;
    }

    std::unordered_map<std::string, unsigned int> wavLengths;
    
    auto& wavFilenames = ir.GetWavFilenames();

    if (wavFilenames.size() > 0)
    {
        LOG_F("\nWill import % .wav %.", wavFilenames.size(), wavFilenames.size() == 1 ? "file" : "files");

        if (!chk.HasChunk(ChunkType::WAV))
        {
            std::vector<char> data;
            data.resize(512 * sizeof(uint32_t));
            chk.AddChunk(std::unique_ptr<IChunk>(new WAVChunk(data, "WAV ")));
        }
    }

    for (auto& filename : wavFilenames)
    {
        auto pathToWav = srcPath.relative_path().remove_filename();
        pathToWav.append(filename);

        auto path = pathToWav.generic_u8string();

        WAVInfo wavInfo;
        if (!wavInfo.ReadInfo(path))
        {
            LOG_EXITERR("(!) Error - failed to read \"%\", make sure it exists and is a valid WAV file", path);
            return 1;
        }

        auto length = wavInfo.GetLengthMilliseconds();
        wavLengths.insert(std::make_pair(filename, length));

        LOG_F("Importing from \"%\" (% seconds)", filename, (float)length / 1000.0f);

        auto& wavBytes = wavInfo.GetData();

        auto mpqFilename = SafePrintf("staredit\\wav\\%", filename);

        try
        {
            mpqWrapper->AddWavFile(path, mpqFilename);
        }
        catch (MPQWrapperException& ex)
        {
            LOG_EXITERR("\n(!) StormLib error: %", ex.what());
            return 1;
        }
    }

    auto& instructions = ir.GetInstructions();

    for (auto& instruction : instructions)
    {
        if (instruction->GetType() == IRInstructionType::PlayWAV)
        {
            auto playWav = (IRPlayWAVInstruction*)instruction.get();
            auto& name = playWav->GetWavName();

            auto stringId = stringsChunk->InsertString(SafePrintf("staredit\\wav\\%", name));

            auto wavChunk = chk.GetFirstChunk<WAVChunk>(ChunkType::WAV);
            auto wavStringId = wavChunk->FindFreeIndex();
            wavChunk->Set(wavStringId, stringId + 1);
            playWav->SetWavStringId(wavStringId);

            if (wavLengths.find(name) == wavLengths.end())
            {
                LOG_F("(!) Warning! WAV length for \"%\" is missing, the file will not play in-game.", name);
                continue;
            }

            playWav->SetWavTime(wavLengths[name]);
        }
        else if (instruction->GetType() == IRInstructionType::Transmission)
        {
            auto transmission = (IRTransmissionInstructrion*)instruction.get();

            auto& name = transmission->GetWavName();
            if (wavLengths.find(name) == wavLengths.end())
            {
                LOG_F("(!) Warning! WAV length for \"%\" is missing, the file will not play in-game.", name);
                continue;
            }

            auto stringId = stringsChunk->InsertString(SafePrintf("staredit\\wav\\%", name));

            auto wavChunk = chk.GetFirstChunk<WAVChunk>(ChunkType::WAV);
            auto wavStringId = wavChunk->FindFreeIndex();
            wavChunk->Set(wavStringId, stringId + 1);
            transmission->SetWavStringId(wavStringId);

            transmission->SetWavTime(wavLengths[name]);
        }
    }

    if (opts.count("dump-ir") > 0)
    {
        LOG_F("\n----------- IR DUMP -----------");
        LOG_F("%", ir.DumpInstructions(true));
        LOG_F("-------------------------------");
    }

    LOG_F("\nEmitted % instructions.\n", instructions.size());

    auto debug = opts.count("debug") > 0 || opts.count("debug-vscode") > 0;

    Compiler compiler(debug);

    auto triggersOwner = 8; // player 1 owns the triggers by default

    if (opts.count("triggers-owner") > 0)
    {
        auto triggerOwner = opts["triggers-owner"].as<unsigned int>();
        if (triggerOwner < 1 || triggerOwner > 8)
        {
            LOG_EXITERR("\n(!) triggers-owner must be between 1 and 8");
            return 1;
        }

        LOG_F("(!) Triggers owner set to % from the command-line", CHK::PlayersByName[triggerOwner - 1]);
    }

    LOG_F("Player % owns the LangUMS triggers.", triggersOwner);
    compiler.SetTriggersOwner(triggersOwner);

    auto triggerOwnerType = ownrChunk->GetPlayerType(triggersOwner - 1);
    if (triggerOwnerType == PlayerType::Human)
    {
        LOG_F("\n(!) Warning! The triggers owner (player %) is currently set to a Human player.", triggersOwner);
        LOG_F("(!) Him leaving the game will break LangUMS in a bad way.\n");

        if (opts.count("force") == 0)
        {
            LOG_F("(!) Setting player % to a Computer player.", triggersOwner);
            LOG_F("(!) If you wish to override this decision call langums.exe again with the --force option.\n");
            ownrChunk->SetPlayerType(triggersOwner - 1, PlayerType::Computer);

            if (chk.HasChunk(ChunkType::IOWN))
            {
                auto iownChunk = chk.GetFirstChunk<IOWNChunk>(ChunkType::IOWN);
                iownChunk->SetPlayerType(triggersOwner - 1, PlayerType::Computer);
            }
        }
    }
    else if (triggerOwnerType != PlayerType::Computer)
    {
        LOG_F("(!) Setting player % to a Computer player because he's the triggers owner. Use --triggers-owner to override.", triggersOwner);

        ownrChunk->SetPlayerType(triggersOwner - 1, PlayerType::Computer);
        if (chk.HasChunk(ChunkType::IOWN))
        {
            auto iownChunk = chk.GetFirstChunk<IOWNChunk>(ChunkType::IOWN);
            iownChunk->SetPlayerType(triggersOwner - 1, PlayerType::Computer);
        }
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

        if (copyBatchSize <= 64)
        {
            LOG_F("(!) WARNING! Copy batch size is set to an extremely low value (<= 64). Arithmetic operations will be VERY slow to execute.");
            LOG_F("(!) Multiplication and division will most likely produce incorrect results.");
        }
        else if (copyBatchSize < 1024)
        {
            LOG_F("(!) WARNING! Copy batch size is set to a moderately low value (< 1024).");
            LOG_F("(!) Arithmetic operations with large numbers will be slow to execute and there are certain limitations to multiplication and division.");
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
        PrintCompilerException(source, ex);
        LOG_EXITERR("");
        return 1;
    }

    LOG_F("Compilation successful! Trigger count: %", triggersChunk->GetTriggersCount());

    std::vector<char> chkBytes;
    chk.Serialize(chkBytes);

    auto compress = opts.count("disable-compression") == 0;
    if (!compress)
    {
        LOG_F("(!) Warning! Compression is disabled. Resulting file size will be much larger than usual.");
    }

    try
    {
        mpqWrapper->WriteFile(SCENARIO_FILENAME, chkBytes, compress);
    }
    catch (MPQWrapperException& ex)
    {
        LOG_EXITERR("\n(!) StormLib error: %", ex.what());
        return 1;
    }

    mpqWrapper = nullptr;

    auto fileSize = std::experimental::filesystem::file_size(tempPath);
    LOG_F("Final size: % kb", fileSize / 1024);

    if (!filesystem::copy_file(tempPath, dstPath, filesystem::copy_options::overwrite_existing, ec))
    {
        LOG_F("\n(!) Failed to create destination file, error code: %", ec);
    }
    else
    {
        LOG_F("Written to: %", dstPath.generic_u8string());
    }

    std::string processName = DEFAULT_PROCESS_NAME;
    if (opts.count("debug-process") > 0)
    {
        processName = opts["debug-process"].as<std::string>();
    }

    if (opts.count("debug") > 0)
    {
        LOG_F("\nStarting a debug session.");

        Debugger debugger(instructions, source, ast);
        if (!debugger.Attach(processName))
        {
            LOG_F("\n(!) Failed to attach to process.");
            return 1;
        }

        DebuggerCli cli(&debugger);
        cli.Repl();
        return 0;
    }

    if (opts.count("debug-vscode") > 0)
    { 
        Debugger debugger(instructions, source, ast);
        if (!debugger.Attach(processName))
        {
            LOG_F("\n(!) Failed to attach to process.");
            return 1;
        }

        vsCodeDebugger->SetDebugger(&debugger);
        vsCodeDebugger->Run();
        return 0;
    }

    LOG_DEINIT();
    return 0;
}
