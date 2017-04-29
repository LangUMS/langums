#include <iostream>
#include <experimental/filesystem>

#include "log.h"
#include "cxxopts.h"
#include "stringutil.h"
#include "stormlib/StormLib.h"
#include "libchk/chk.h"
#include "parser/preprocessor.h"
#include "parser/parser.h"
#include "parser/template_instantiator.h"
#include "parser/ast_optimizer.h"
#include "compiler/ir.h"
#include "compiler/compiler.h"
#include "compiler/registermap_parser.h"
#include "wavinfo.h"

#undef min
#undef max

#define VERSION "v0.1.0"

using namespace Langums;
using namespace CHK;
using namespace std::experimental;

#define SCENARIO_FILENAME "staredit\\scenario.chk"

int main(int argc, char* argv[])
{
    cxxopts::Options opts("LangUMS " VERSION, "LangUMS compiler");

    opts.add_options()
        ("h,help", "Prints this help message", cxxopts::value<bool>())
        ("s,src", "Source .scx map file", cxxopts::value<std::string>())
        ("d,dst", "Destination .scx map file", cxxopts::value<std::string>())
        ("l,lang", "Source .l script file", cxxopts::value<std::string>())
        ("r,reg", "Available registers map file", cxxopts::value<std::string>())
        ("strip", "Strips unnecessary data from the resulting .scx. Will make the file unopenable in editors.", cxxopts::value<bool>())
        ("preserve-triggers", "Preserves already existing triggers in the map (use with caution!).", cxxopts::value<bool>())
        ("copy-batch-size", "Maximum number value that can be copied in one cycle. Must be power of 2. Higher values will increase the amount of emitted triggers (default: 65536).", cxxopts::value<unsigned int>())
        ("triggers-owner", "The index of the player which holds the main logic triggers (default: 1).", cxxopts::value<unsigned int>())
        ("disable-optimization", "Disables all forms of compiler optimization (useful to debug compiler issues).", cxxopts::value<bool>())
        ("dump-ir", "Dumps the intermediate representation during compilation.", cxxopts::value<bool>())
        ("force", "Forces the compiler to do thing it shouldn't.", cxxopts::value<bool>())
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
    LOG_F("");

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

    HANDLE mpq = nullptr;
    auto mpqPath = tempPath.generic_u8string();
    if (!SFileOpenArchive(mpqPath.c_str(), 0, 0, &mpq))
    {
        auto errorCode = GetLastError();
        LOG_EXITERR("\n(!) SFileOpenArchive failed, error code: %. Looks like an invalid scx file.", errorCode);
        return 1;
    }

    LOG_F("Map looks like a valid MPQ file.");
     
    HANDLE scenarioFile = nullptr;
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

    LOG_F("Reading from scenario.chk (% bytes).", scenarioFileSize);

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

    auto& triggersChunk = chk.GetFirstChunk<CHKTriggersChunk>("TRIG");
    triggersChunk = chk.GetFirstChunk<CHKTriggersChunk>("TRIG");

    auto& stringsChunk = chk.GetFirstChunk<CHKStringsChunk>("STR ");

    if (!chk.HasChunk("WAV "))
    {
        std::vector<char> data;
        data.resize(512 * sizeof(uint32_t));

        chk.AddChunk("WAV ", std::unique_ptr<IChunk>(new CHKWavChunk(data, "WAV ")));
    }

    auto& wavChunk = chk.GetFirstChunk<CHKWavChunk>("WAV ");

    auto preserveTriggers = opts.count("preserve-triggers") > 0;

    LOG_F("Existing trigger count: % (%)", triggersChunk.GetTriggersCount(), preserveTriggers ? "preserved" : "not preserved");

    auto& dimChunk = chk.GetFirstChunk<CHKDimChunk>("DIM ");
    LOG_F("Map size: %x%", dimChunk.GetWidth(), dimChunk.GetHeight());
    
    auto& tilesetChunk = chk.GetFirstChunk<CHKTilesetChunk>("ERA ");
    LOG_F("Tileset: %", tilesetChunk.GetTilesetTypeString());

    auto& ownrChunk = chk.GetFirstChunk<CHKOwnrChunk>("OWNR");

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

    std::unordered_map<std::string, unsigned int> wavLengths;
    
    auto& wavFilenames = ir.GetWavFilenames();

    if (wavFilenames.size() > 0)
    {
        LOG_F("\nWill import % .wav %.", wavFilenames.size(), wavFilenames.size() == 1 ? "file" : "files");
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
        if (!SFileAddWave(mpq, path.c_str(), mpqFilename.c_str(), MPQ_COMPRESSION_ADPCM_STEREO, MPQ_WAVE_QUALITY_HIGH))
        {
            LOG_EXITERR("(!) Error - failed to add \"%\" to MPQ archive.", path);
            return 1;
        }
        else
        {
            LOG_F("Successfully added \"%\" to MPQ archive.", filename);
        }
    }

    auto& instructions = ir.GetInstructions();

    for (auto& instruction : instructions)
    {
        if (instruction->GetType() == IRInstructionType::PlayWAV)
        {
            auto playWav = (IRPlayWAVInstruction*)instruction.get();
            auto& name = playWav->GetWavName();

            auto stringId = stringsChunk.InsertString(SafePrintf("staredit\\wav\\%", name));
            auto wavStringId = wavChunk.FindFreeIndex();
            wavChunk.Set(wavStringId, stringId + 1);
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

            auto stringId = stringsChunk.InsertString(SafePrintf("staredit\\wav\\%", name));
            auto wavStringId = wavChunk.FindFreeIndex();
            wavChunk.Set(wavStringId, stringId + 1);
            transmission->SetWavStringId(wavStringId);

            transmission->SetWavTime(wavLengths[name]);
        }
    }

    if (opts.count("dump-ir") > 0)
    {
        LOG_F("\n----------- IR DUMP -----------");
        LOG_F("%", ir.DumpInstructions(true));
        LOG_F("-------------------------------\n");
    }

    LOG_F("\nEmitted % instructions.\n", instructions.size());

    Compiler compiler;

    auto triggersOwner = 8; // player 8 owns the triggers by default

    if (opts.count("triggers-owner") > 0)
    {
        auto triggerOwner = opts["triggers-owner"].as<unsigned int>();
        if (triggerOwner < 1 || triggerOwner > 8)
        {
            LOG_EXITERR("\n(!) triggers-owner must be between 1 and 8");
            return 1;
        }

        LOG_F("(!) Triggers owner set to % from command-line", CHK::PlayersByName[triggerOwner - 1]);
    }

    LOG_F("Player % owns the LangUMS triggers.", triggersOwner);
    compiler.SetTriggersOwner(triggersOwner);

    auto triggerOwnerType = ownrChunk.GetPlayerType(triggersOwner - 1);
    if (triggerOwnerType == PlayerType::Human)
    {
        LOG_F("\n(!) Warning! The triggers owner (player %) is currently set to a Human player.", triggersOwner);
        LOG_F("(!) Him leaving the game will break LangUMS in a bad way.\n");

        if (opts.count("force") == 0)
        {
            LOG_F("(!) Setting player % to a Computer player.", triggersOwner);
            LOG_F("(!) If you wish to override this decision call langums.exe again with the --force option.\n");
            ownrChunk.SetPlayerType(triggersOwner - 1, PlayerType::Computer);
        }
    }
    else if (triggerOwnerType != PlayerType::Computer)
    {
        ownrChunk.SetPlayerType(triggersOwner - 1, PlayerType::Computer);
        LOG_F("(!) Setting player % to a Computer player because he's the triggers owner. Use --triggers-owner to override.", triggersOwner);
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

    if (!SFileCreateFile(mpq, SCENARIO_FILENAME, 0, chkBytes.size(), 0, MPQ_FILE_REPLACEEXISTING, &scenarioFile))
    {
        LOG_EXITERR("Failed to open scenario.chk for writing.");
        return 1;
    }

    if (!SFileWriteFile(scenarioFile, chkBytes.data(), chkBytes.size(), MPQ_COMPRESSION_ZLIB))
    {
        LOG_EXITERR("Failed to write out scenario.chk to MPQ archive.");
        return 1;
    }

    SFileCloseFile(scenarioFile);
    scenarioFile = nullptr;
    SFileCloseArchive(mpq);
    mpq = nullptr;

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
