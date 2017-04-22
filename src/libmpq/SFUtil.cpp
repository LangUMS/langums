// License information for this code is in license.txt

#include <windows.h>
#include <stdlib.h>
#include "SFTypes.h"

void WINAPI SFMemZero(LPVOID lpvDestination, DWORD dwLength)
{
	DWORD dwPrevLen = dwLength;
	LPDWORD lpdwDestination = (LPDWORD)lpvDestination;
	LPBYTE lpbyDestination;

	dwLength >>= 2;

	while (dwLength--)
		*lpdwDestination++ = 0;

	lpbyDestination = (LPBYTE)lpdwDestination;

	dwLength = dwPrevLen;
	dwLength &= 3;

	while (dwLength--)
		*lpbyDestination++ = 0;
}

LPVOID WINAPI SFAlloc(DWORD dwSize)
{
	LPVOID lpMemory = malloc(dwSize);
	if (lpMemory) SFMemZero(lpMemory,dwSize);
	return lpMemory;
}

void WINAPI SFFree(LPVOID lpvMemory)
{
	if (lpvMemory) free(lpvMemory);
}

Int64 SFGetFileSize(HANDLE hFile)
{
	IntConv FileSize;

	FileSize.ui64 = 0;

	FileSize.ui32[0] = ::GetFileSize(hFile, &FileSize.ui32[1]);

	if (FileSize.ui32[0] == INVALID_FILE_SIZE) {
		if (::GetLastError() != NO_ERROR)
			return -1;
	}

	return FileSize.i64;
}

Int64 SFSetFilePointer(HANDLE hFile, Int64 nDistance, UInt32 dwMoveMethod)
{
	IntConv FilePos;

	FilePos.i64 = nDistance;

	FilePos.i32[0] = ::SetFilePointer(hFile, FilePos.i32[0], &FilePos.i32[1], dwMoveMethod);

#ifdef INVALID_SET_FILE_POINTER
	if (FilePos.ui32[0] == INVALID_SET_FILE_POINTER) {
#else
	if (FilePos.ui32[0] == INVALID_FILE_SIZE) {
#endif
		if (::GetLastError() != NO_ERROR)
			return -1;
	}

	return FilePos.i64;
}

size_t strlnlen(const char *strline)
{
	if (strline==0) return 0;
	const char *strcr = strchr(strline,'\r');
	const char *strlf = strchr(strline,'\n');
	if (strcr==0 && strlf==0) return strlen(strline);
	if (strcr!=0 && (strcr<strlf || strlf==0)) return strcr-strline;
	if (strlf!=0 && (strlf<strcr || strcr==0)) return strlf-strline;
	return strlen(strline);
}


const char *nextline(const char *strline)
{
	if (strline==0) return 0;
	const char *strcr = strchr(strline,'\r');
	const char *strlf = strchr(strline,'\n');
	if (strcr==0 && strlf==0) return 0;
	const char *streol = strlf;
	if (strcr!=0 && (strcr<strlf || strlf==0)) streol = strcr;
	if (strlf!=0 && (strlf<strcr || strcr==0)) streol = strlf;
	do {
		streol++;
	} while (streol[0]=='\r' || streol[0]=='\n');
	if (streol[0]==0) return 0;
	return streol;
}

char *nextline(char *strline)
{
	return (char*)nextline(strline);
}
