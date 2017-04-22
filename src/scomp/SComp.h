// SComp.h - Header for main compression/decompression routines
// License information for this code is in license.txt

#ifndef S_COMP_INCLUDED
#define S_COMP_INCLUDED

#if (defined(_WIN32) || defined(WIN32)) && !defined(NO_WINDOWS_H)
#include <windows.h>
#else
#include "wintypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern const unsigned int UNSUPPORTED_COMPRESSION;
extern const unsigned int UNSUPPORTED_DECOMPRESSION;

BOOL WINAPI SCompCompress(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, DWORD dwCompressionType, DWORD dwCompressionSubType, DWORD dwCompressLevel);
BOOL WINAPI SCompDecompress(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize);

#ifdef __cplusplus
};  // extern "C" 
#endif

#endif

