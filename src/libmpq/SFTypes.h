// License information for this code is in license.txt

#ifndef SFTYPES_INCLUDED
#define SFTYPES_INCLUDED

#if defined(_WIN32) || defined(_WIN64)

typedef signed char    Int8;
typedef signed short   Int16;
typedef signed long    Int32;
typedef signed __int64 Int64;

#ifdef _WIN64
typedef signed __int64 IntPtr;
#else
typedef signed int     IntPtr;
#endif

typedef unsigned char    UInt8;
typedef unsigned short   UInt16;
typedef unsigned long    UInt32;
typedef unsigned __int64 UInt64;

#ifdef _WIN64
typedef unsigned __int64 UIntPtr;
#else
typedef unsigned int     UIntPtr;
#endif

#else

#include <stdint.h>

typedef int8_t   Int8;
typedef int16_t  Int16;
typedef int32_t  Int32;
typedef int64_t  Int64;
typedef intptr_t IntPtr;

typedef uint8_t   UInt8;
typedef uint16_t  UInt16;
typedef uint32_t  UInt32;
typedef uint64_t  UInt64;
typedef uintptr_t UIntPtr;

#endif

union IntConv {
	Int8 i8[8];
	UInt8 ui8[8];
	Int16 i16[4];
	UInt16 ui16[4];
	Int32 i32[2];
	UInt32 ui32[2];
	Int64 i64;
	UInt64 ui64;
};

#endif // #ifndef SFTYPES_INCLUDED

