// SMem.cpp - Memory functions
//
// Converted from assembly to C++ by ShadowFlare.
// E-mail  : blakflare@hotmail.com
// Webpage : http://sfsrealm.hopto.org/
// License information for this code is in license.txt


#include "SMem.h"
#include <stdlib.h>

LPVOID WINAPI SMemAlloc(DWORD dwSize)
{
	LPVOID lpMemory = malloc(dwSize);
	if (lpMemory) SMemZero(lpMemory,dwSize);
	return lpMemory;
}

void WINAPI SMemFree(LPVOID lpvMemory)
{
	if (lpvMemory) free(lpvMemory);
}

DWORD WINAPI SMemCopy(LPVOID lpDestination, LPCVOID lpSource, DWORD dwLength)
{
	DWORD dwPrevLen = dwLength;
	LPDWORD lpdwDestination = (LPDWORD)lpDestination,lpdwSource = (LPDWORD)lpSource;
	LPBYTE lpbyDestination,lpbySource;

	dwLength >>= 2;

	while (dwLength--)
		*lpdwDestination++ = *lpdwSource++;

	lpbyDestination = (LPBYTE)lpdwDestination;
	lpbySource = (LPBYTE)lpdwSource;

	dwLength = dwPrevLen;
	dwLength &= 3;

	while (dwLength--)
		*lpbyDestination++ = *lpbySource++;

	return dwPrevLen;
}

void WINAPI SMemZero(LPVOID lpDestination, DWORD dwLength)
{
	DWORD dwPrevLen = dwLength;
	LPDWORD lpdwDestination = (LPDWORD)lpDestination;
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

