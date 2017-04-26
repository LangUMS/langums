#include <iostream>
#include <experimental/filesystem>

#include "log.h"
#include "cxxopts.h"
#include "stringutil.h"
#include "libmpq/SFmpqapi.h"
#include "libchk/chk.h"
#include "parser/preprocessor.h"
#include "parser/parser.h"
#include "compiler/ir.h"
#include "compiler/compiler.h"

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
		("strip", "Strips unnecessary data from the resulting .scx. Will make the file unopenable in editors.", cxxopts::value<bool>())
		("preserve-triggers", "Preserves already existing triggers in the map. (DANGEROUS, USE WITH CAUTION)", cxxopts::value<bool>())
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

	LOG_F("Source: %", srcPath);
	LOG_F("Destination: %", dstPath);
	LOG_F("Script: %", langPath);

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
	script = preprocessor.Process(script);

	Parser parser;
	std::shared_ptr<IASTNode> ast;
	
	try
	{
		ast = std::shared_ptr<IASTNode>(parser.Parse(script));
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
		ir.Optimize();
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

	auto preserveTriggers = opts.count("preserve-triggers") > 0;

	try
	{
		compiler.Compile(instructions, chk, preserveTriggers);
	}
	catch (const CompilerException& ex)
	{
		LOG_EXITERR("\n(!) CodeGen error: %", ex.what());
		return 1;
	}

	auto& triggersChunk = chk.GetFirstChunk<CHKTriggersChunk>("TRIG");
	triggersChunk = chk.GetFirstChunk<CHKTriggersChunk>("TRIG");
	LOG_F("Compilation successful! Trigger count: %.", triggersChunk.GetTriggersCount());

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
