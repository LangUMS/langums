#ifdef _WIN32

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#define PSAPI_VERSION 1
#include <psapi.h>
#include <tlhelp32.h>

#include "process_util.h"
#include "../log.h"

#pragma comment(lib, "psapi.lib")

typedef LONG (NTAPI *NtSuspendProcess)(IN HANDLE ProcessHandle);
typedef LONG (NTAPI *NtResumeProcess)(IN HANDLE ProcessHandle);

auto g_SuspendProcess = (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtSuspendProcess");
auto g_ResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtResumeProcess");

namespace Langums
{

    using namespace Process;

    ProcessHandle Process::OpenByName(const std::string& processName, void*& baseAddress)
    {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

        ProcessHandle handle = nullptr;

        if (Process32First(snapshot, &processEntry))
        {
            while (Process32Next(snapshot, &processEntry))
            {
                if (_stricmp(processEntry.szExeFile, processName.c_str()) == 0)
                {
                    handle = OpenProcess(PROCESS_ALL_ACCESS, false, processEntry.th32ProcessID);
                    break;
                }
            }
        }

        CloseHandle(snapshot);
        
        if (!handle)
        {
            return nullptr;
        }

        HMODULE hMod;
        DWORD cbNeeded;
        TCHAR szProcessName[MAX_PATH];

        if (EnumProcessModulesEx(handle, &hMod, sizeof(hMod), &cbNeeded, LIST_MODULES_32BIT))
        {
            GetModuleBaseName(handle, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));
            if (!_tcsicmp("StarCraft.exe", szProcessName))
            {
                baseAddress = hMod;
            }
        }

        if (baseAddress == nullptr)
        {
            return nullptr;
        }

        return handle;
    }

    bool Process::Close(ProcessHandle process)
    {
        return CloseHandle(process);
    }

    bool Process::ReadMemory(ProcessHandle process, void* address, size_t size, void* dstBuffer, size_t& retBytesRead)
    {
        SIZE_T bytesRead = 0u;
        if (!ReadProcessMemory(process, address, dstBuffer, size, &bytesRead))
        {
            auto error = GetLastError();
            return false;
        }

        retBytesRead = bytesRead;
        return true;
    }
    
    bool Process::WriteMemory(ProcessHandle process, void* address, size_t size, void* srcBuffer, size_t& retBytesWritten)
    {
        SIZE_T bytesWritten;
        if (!WriteProcessMemory(process, address, srcBuffer, size, &bytesWritten))
        {
            return false;
        }

        retBytesWritten = bytesWritten;
        return true;
    }

    void Process::Suspend(ProcessHandle process)
    {
        g_SuspendProcess(process);
    }

    void Process::Resume(ProcessHandle process)
    {
        g_ResumeProcess(process);
    }

    bool Process::IsRunning(ProcessHandle process)
    {
        DWORD exitCode;
        if (!GetExitCodeProcess(process, &exitCode))
        {
            return false;
        }

        return exitCode != STILL_ACTIVE;
    }

}

#endif
