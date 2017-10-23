#include <unordered_map>
#include <iostream>
#include <filesystem>

#include "../cxxopts.h"
#include "../log.h"
#include "../log_interface_stdout.h"
#include "../log_interface_file.h"

#include "hashutil.h"
#include "fileutil.h"

using namespace Langums;
using namespace std::experimental;

// using system() is bad. TODO: portable wrapper for CreateProcess
bool Compile(const std::string& langPath, const std::string& dstPath, const std::string& irPath, const std::string& logPath)
{
    auto cmd = SafePrintf("langums.exe --lang % --dst % --ir % --log-file % --quiet --dump-ir", langPath, dstPath, irPath, logPath);
    return system(cmd.c_str()) == 0;
}

int main(int argc, char* argv[])
{
    Log::Instance()->AddInterface(std::unique_ptr<ILogInterface>(new LogInterfaceStdout()));
    Log::Instance()->AddInterface(std::unique_ptr<ILogInterface>(new LogInterfaceFile("test-runner.log")));

    cxxopts::Options opts("LangUMS test runner", "LangUMS test runner");

    opts.add_options()
        ("h,help", "Prints this help message.", cxxopts::value<bool>())
        ("t,tests", "Path to tests directory.", cxxopts::value<std::string>())
        ("i,init", "Creates the initial .ir and .sha256 test files.", cxxopts::value<bool>())
        ;

    try
    {
        opts.parse(argc, argv);
    }
    catch (...)
    {
        LOG_EXITERR("Invalid arguments. %", opts.help());
        return 1;
    }

    if (opts.count("help") > 0)
    {
        LOG_EXITERR("%", opts.help());
        return 0;
    }

    if (opts.count("tests") != 1)
    {
        LOG_F("%", opts.help());
        LOG_EXITERR("\n(!) Missing --tests argument.");
        return 1;
    }

    auto testsPath = filesystem::path(opts["tests"].as<std::string>());
    if (!filesystem::is_directory(testsPath))
    {
        LOG_EXITERR("\n(!) Invalid tests directory \"%\"", testsPath);
        return 1;
    }

    auto tmpPath = filesystem::temp_directory_path();

    if (opts.count("init") != 0)
    {
        LOG_F("Creating initial test data.");

        for (auto& filename : filesystem::directory_iterator(testsPath))
        {
            if (filename.path().extension() != ".l")
            {
                continue;
            }

            LOG_F("Compiling %", filename);
            auto langPath = filename.path().generic_u8string();
            auto dstPath = (tmpPath / filename.path().filename().replace_extension("scx")).generic_u8string();
            auto tmp = filename.path();
            auto irPath = tmp.replace_extension("ir").generic_u8string();
            auto logPath = (tmpPath / filename.path().filename().replace_extension("log")).generic_u8string();

            if (!Compile(langPath, dstPath, irPath, logPath))
            {
                LOG_F("Compilation failed for \"%\"", filename);
                return 1;
            }

            std::string hash;
            if (!GetFileHash(dstPath, hash))
            {
                LOG_F("Failed to compute hash for \"%\"", dstPath);
                return 1;
            }

            LOG_F("SCX hash: %", hash);

            auto hashPath = tmp.replace_extension("sha256").generic_u8string();
            if (!WriteTextFile(hashPath, hash))
            {
                LOG_F("Failed to write to \"%\"", hashPath);
                return 1;
            }
        }

        return 0;
    }

    LOG_F("Running tests...");

    std::vector<std::string> tested;
    std::vector<std::string> failed;
    std::unordered_map<std::string, std::string> logs;

    for (auto& filename : filesystem::directory_iterator(testsPath))
    {
        if (filename.path().extension() != ".l")
        {
            continue;
        }

        auto testName = filename.path().stem().generic_u8string();

        LOG_F("- Testing \"%\"", filename);
        tested.push_back(testName);

        auto tmp = filename.path();
        auto irPath = tmp.replace_extension("ir").generic_u8string();
        auto hashPath = tmp.replace_extension("sha256").generic_u8string();

        std::string expectedIR;
        if (!ReadTextFile(irPath, expectedIR))
        {
            LOG_F("Failed to read IR from \"%\"", irPath);
            failed.push_back(testName);
            continue;
        }

        std::string expectedHash;
        if (!ReadTextFile(hashPath, expectedHash))
        {
            LOG_F("Failed to read hash from \"%\"", hashPath);
            failed.push_back(testName);
            continue;
        }

        auto langPath = filename.path().generic_u8string();

        auto dstPath = (tmpPath / filename.path().filename().replace_extension("scx")).generic_u8string();
        auto dstIrPath = (tmpPath / filename.path().filename().replace_extension("ir")).generic_u8string();
        auto logPath = (tmpPath / filename.path().filename().replace_extension("log")).generic_u8string();

        if (!Compile(langPath, dstPath, dstIrPath, logPath))
        {
            LOG_F("Compilation failed for \"%\"", filename);
            failed.push_back(testName);
            continue;
        }

        if (!ReadTextFile(logPath, logs[testName]))
        {
            LOG_F("Failed to read from \"%\"", logPath);
            failed.push_back(testName);
            continue;
        }

        std::string hash;
        if (!GetFileHash(dstPath, hash))
        {
            LOG_F("Failed to compute hash for \"%\"", dstPath);
            failed.push_back(testName);
            continue;
        }

        std::string ir;
        if (!ReadTextFile(dstIrPath, ir))
        {
            LOG_F("Failed to read from \"%\"", dstIrPath);
            failed.push_back(testName);
            continue;
        }

        if (ir != expectedIR)
        {
            LOG_F("(!) IR mismatch for \"%\"", testName);

            LOG_F("*** EXPECTED ***");
            LOG_F("%", expectedIR);
            LOG_F("****************");

            LOG_F("*** ACTUAL ***");
            LOG_F("%", ir);
            LOG_F("**************");

            failed.push_back(testName);
            continue;
        }

        if (hash != expectedHash)
        {
            LOG_F("(!) Hash mismatch for \"%\", expected %, got %", testName, expectedHash, hash);
            failed.push_back(testName);
            continue;
        }
    }

    LOG_F("");
    LOG_F("***************************");
    LOG_F("RAN % TESTS", tested.size());
    LOG_F("SUCCESSFUL: %", tested.size() - failed.size());
    LOG_F("FAILED: %", failed.size());
    LOG_F("***************************");

    if (!failed.empty())
    {
        LOG_F("Failing tests:");

        for (auto& testName : failed)
        {
            auto logPath = SafePrintf("%.compiler.log", testName);

            if (!WriteTextFile(logPath, logs[testName]))
            {
                LOG_F("(!) Failed to write to \"%\"", logPath);
            }

            LOG_F("- % - compiler output: %", testName, logPath);
        }
    }

    LOG_DEINIT();

    return failed.empty() ? 0 : 1;
}
