// License information for this code is in license.txt

#ifndef SFUTIL_INCLUDED
#define SFUTIL_INCLUDED

#include <windows.h>
#include "SFTypes.h"

LPVOID WINAPI SFAlloc(DWORD dwSize);
void WINAPI SFFree(LPVOID lpvMemory);
void WINAPI SFMemZero(LPVOID lpvDestination, DWORD dwLength);
Int64 SFGetFileSize(HANDLE hFile);
Int64 SFSetFilePointer(HANDLE hFile, Int64 nDistance, UInt32 dwMoveMethod);
size_t strlnlen(const char *strline);
const char *nextline(const char *strline);
char *nextline(char *strline);

#endif // #ifndef SFUTIL_INCLUDED

