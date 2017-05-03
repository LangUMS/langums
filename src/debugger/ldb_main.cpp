#include <iostream>

#include "../log.h"
#include "../cxxopts.h"
#include "../stringutil.h"
#include "../ast/ast.h"

#include "process_util.h"
#include "memsearch.h"
#include "reg_table.h"

#undef min
#undef max

#define VERSION "v0.1.0"

#define DEFAULT_PROCESS_NAME "starcraft.exe"
#define BASE_OFFSET 0x00e50000

using namespace Langums;

int main(int argc, char* argv[])
{
    cxxopts::Options opts("LDB " VERSION, "LangUMS debugger");

    opts.add_options()
        ("h,help", "Prints this help message", cxxopts::value<bool>())
        ("p,process-name", "Name of the game's process (default: starcraft.exe)", cxxopts::value<std::string>())
        ;
    opts.parse(argc, argv);

    if (opts.count("help") > 0)
    {
        LOG_F("%", opts.help());
        Log::Log::Instance()->Destroy();
        return 0;
    }

    std::string processName = DEFAULT_PROCESS_NAME;
    if (opts.count("process-name") > 0)
    {
        processName = opts["process-name"].as<std::string>();
    }

    LOG_F("Looking for %", processName);
    
    void* baseAddress;
    auto gameHandle = Process::OpenByName(processName, baseAddress);
    if (gameHandle == nullptr)
    {
        LOG_EXITERR("Failed to find %, bailing out...", processName);
        return 1;
    }

    LOG_F("Process found.");

    
    RegTable registers(gameHandle, baseAddress);
    registers.Update();

    LOG_DEINIT();
    return 0;
}
