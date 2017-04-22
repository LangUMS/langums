// SMem.h - Header for memory functions
// License information for this code is in license.txt

#ifndef S_MEM_INCLUDED
#define S_MEM_INCLUDED

#if (defined(_WIN32) || defined(WIN32)) && !defined(NO_WINDOWS_H)
#include <windows.h>
#else
#include "wintypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

LPVOID WINAPI SMemAlloc(DWORD dwSize);
void WINAPI SMemFree(LPVOID lpvMemory);
DWORD WINAPI SMemCopy(LPVOID lpDestination, LPCVOID lpSource, DWORD dwLength);
void WINAPI SMemZero(LPVOID lpDestination, DWORD dwLength);

#ifdef __cplusplus
};  // extern "C" 
#endif

#endif

