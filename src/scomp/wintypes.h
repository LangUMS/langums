// wintypes.h - Windows data types
// (when not compiling for Windows)
// License information for this code is in license.txt

#ifndef WIN_TYPES_INCLUDED
#define WIN_TYPES_INCLUDED

#include <stdint.h>

#ifndef WINAPI
#define WINAPI
#endif
#define CDECL
#define __fastcall

#define CONST const
#define VOID void

//typedef void VOID;
typedef void *LPVOID;
typedef CONST void *LPCVOID;
typedef const char *LPCSTR;
typedef uint8_t BYTE;
typedef BYTE *LPBYTE;
typedef uint16_t WORD;
typedef WORD *LPWORD;
typedef int16_t SHORT;
typedef uint32_t DWORD;
typedef DWORD *LPDWORD;
typedef int32_t LONG;
typedef LONG *LPLONG;

typedef int BOOL;
typedef int (WINAPI *FARPROC)();

#define FALSE 0
#define TRUE 1

#define ERROR_NOT_ENOUGH_MEMORY          8L
#define ERROR_INVALID_PARAMETER          87L

#endif

