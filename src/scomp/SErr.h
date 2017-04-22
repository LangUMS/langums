// SErr.h - Header for SetLastError routine
// License information for this code is in license.txt

#ifndef S_ERR_INCLUDED
#define S_ERR_INCLUDED

#if (defined(_WIN32) || defined(WIN32)) && !defined(NO_WINDOWS_H)
#include <windows.h>
#else
#include "wintypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

DWORD WINAPI SErrSetLastError(DWORD dwErrorCode);
DWORD WINAPI SErrGetLastError(VOID);

#ifdef __cplusplus
};  // extern "C" 
#endif

#endif

