// SErr.cpp - SetLastError routine
//
// Converted from assembly to C++ by ShadowFlare.
// E-mail  : blakflare@hotmail.com
// Webpage : http://sfsrealm.hopto.org/
// License information for this code is in license.txt


#include "SErr.h"

DWORD dwLastError;

#if (defined(_WIN32) || defined(WIN32)) && !defined(NO_WINDOWS_H)

DWORD WINAPI SErrSetLastError(DWORD dwErrorCode)
{
	dwLastError = dwErrorCode;
	SetLastError(dwErrorCode);
	return dwErrorCode;
}

#else

DWORD WINAPI SErrSetLastError(DWORD dwErrorCode)
{
	return (dwLastError = dwErrorCode);
}

#endif

DWORD WINAPI SErrGetLastError(VOID)
{
	return dwLastError;
}

