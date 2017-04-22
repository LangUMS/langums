// SComp.cpp - Main compression/decompression and wrapper routines
//
// Requires Zlib for Warcraft III files (not included).
// Get it at http://www.gzip.org/zlib/
//
// Converted from assembly to C++ by ShadowFlare.
// E-mail  : blakflare@hotmail.com
// Webpage : http://sfsrealm.hopto.org/
// License information for this code is in license.txt


// Define NO_WINDOWS_H globally if compiling for
// Windows to disable calling Windows functions.

// Comment this out to disable Zlib compression support.
#define USE_ZLIB

// Comment this out to disable bzip 2 compression support.
#define USE_BZIP2

#include "SComp.h"
#include "SErr.h"
#include "SMem.h"
#include "wave.h"
#include "huffman.h"

#ifdef USE_ZLIB
#ifndef __SYS_ZLIB
#define ZLIB_INTERNAL
#include "zlib/zlib.h"
#else
#include <zlib.h>
#endif
#endif

#include "pklib.h"

#ifdef USE_BZIP2
#ifndef __SYS_BZLIB
#include "bzip2/bzlib.h"
#else
#include <bzlib.h>
#endif
#endif

typedef void (__fastcall *FCOMPRESS)(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel);
typedef void (__fastcall *FDECOMPRESS)(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize);

struct CompressFunc {
	DWORD dwFlag;
	FCOMPRESS fnCompress;
};

struct DecompressFunc {
	DWORD dwFlag;
	FDECOMPRESS fnDecompress;
};

void __fastcall CompressWaveMono(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel);
void __fastcall CompressWaveStereo(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel);
void __fastcall HuffmanCompress(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel);
#ifdef USE_ZLIB
void __fastcall Deflate(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel);
#endif
void __fastcall Implode(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel);
#ifdef USE_BZIP2
void __fastcall CompressBZ2(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel);
#endif

void __fastcall DecompressWaveMono(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize);
void __fastcall DecompressWaveStereo(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize);
void __fastcall HuffmanDecompress(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize);
#ifdef USE_ZLIB
void __fastcall Inflate(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize);
#endif
void __fastcall Explode(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize);
#ifdef USE_BZIP2
void __fastcall DecompressBZ2(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize);
#endif

void __fastcall InitWaveCompress(DWORD dwCompressLevel, LPDWORD *lplpdwCompressionSubType, LPDWORD lpdwCompressionSubType);

typedef struct {
	LPVOID lpvDestinationMem;
	DWORD dwDestStart;
	DWORD dwDestLen;
	LPVOID lpvSourceMem;
	DWORD dwSrcStart;
	DWORD dwSrcLen;
} BUFFERINFO;

unsigned int FillInput(char *lpvBuffer, unsigned int *lpdwSize, void *param);
void FillOutput(char *lpvBuffer, unsigned int *lpdwSize, void *param);

const unsigned int UNSUPPORTED_COMPRESSION   = (0xFF ^ (0x40 | 0x80 | 0x01
#ifdef USE_ZLIB
	| 0x02
#endif
	| 0x08
#ifdef USE_BZIP2
	| 0x10
#endif
	));
const unsigned int UNSUPPORTED_DECOMPRESSION = (0xFF ^ (0x40 | 0x80 | 0x01
#ifdef USE_ZLIB
	| 0x02
#endif
	| 0x08
#ifdef USE_BZIP2
	| 0x10
#endif
	));

CompressFunc CompressionFunctions[] =
{
	{0x40, CompressWaveMono},
	{0x80, CompressWaveStereo},
	{0x01, HuffmanCompress},
#ifdef USE_ZLIB
	{0x02, Deflate},
#endif
	{0x08, Implode},
#ifdef USE_BZIP2
	{0x10, CompressBZ2},
#endif
};

DecompressFunc DecompressionFunctions[] =
{
	{0x40, DecompressWaveMono},
	{0x80, DecompressWaveStereo},
	{0x01, HuffmanDecompress},
#ifdef USE_ZLIB
	{0x02, Inflate},
#endif
	{0x08, Explode},
#ifdef USE_BZIP2
	{0x10, DecompressBZ2},
#endif
};

const DWORD nCompFunctions = sizeof(CompressionFunctions) / sizeof(CompressionFunctions[0]);
const DWORD nDecompFunctions = sizeof(DecompressionFunctions) / sizeof(DecompressionFunctions[0]);

CompressFunc *lpLastCompressMethod = &CompressionFunctions[nCompFunctions-1];
DecompressFunc *lpLastDecompressMethod = &DecompressionFunctions[nDecompFunctions-1];

BOOL WINAPI SCompCompress(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, DWORD dwCompressionType, DWORD dwCompressionSubType, DWORD dwCompressLevel)
{
	DWORD dwCompressCount,dwCurFlag,dwTempFlags,dwInSize,dwOutSize;
	LPVOID lpvInBuffer,lpvOutBuffer,lpvWorkBuffer = nullptr,lpvAllocBuffer;
	CompressFunc *lpCFuncTable;

	if (!lpdwCompressedSize) {
	  SErrSetLastError(ERROR_INVALID_PARAMETER);
	  return FALSE;
	}
	if (*lpdwCompressedSize < dwDecompressedSize || !lpvDestinationMem || !lpvSourceMem) {
	  SErrSetLastError(ERROR_INVALID_PARAMETER);
	  return FALSE;
	}

	dwCompressCount = 0;
	dwTempFlags = dwCompressionType;
	lpCFuncTable = CompressionFunctions;

	do {
	  dwCurFlag = lpCFuncTable->dwFlag;

	  if (dwCompressionType & dwCurFlag) {
	    dwCompressCount++;
	  }

	  lpCFuncTable++;
	  dwTempFlags &= ~dwCurFlag;
	} while (lpCFuncTable <= lpLastCompressMethod);

	if (dwTempFlags) {
	  return FALSE;
	}

	dwInSize = dwDecompressedSize;
	lpvAllocBuffer = 0;

	if (dwCompressCount >= 2
	  || !((LPBYTE)lpvDestinationMem+dwInSize <= (LPBYTE)lpvSourceMem
	    || (LPBYTE)lpvSourceMem+dwInSize <= (LPBYTE)lpvDestinationMem
	    || !dwCompressCount))
	{
	  lpvWorkBuffer = SMemAlloc(dwInSize);

	  if (!lpvWorkBuffer) {
	    SErrSetLastError(ERROR_NOT_ENOUGH_MEMORY);
	    return FALSE;
	  }

	  lpvAllocBuffer = lpvWorkBuffer;
	}

	lpvInBuffer = lpvSourceMem;
	lpCFuncTable = CompressionFunctions;

	do {
	  dwTempFlags = dwCompressionType;
	  dwCurFlag = lpCFuncTable->dwFlag;

	  if (dwTempFlags & dwCurFlag) {
	    dwCompressCount--;

	    if (dwCompressCount & 1) {
	      lpvOutBuffer = lpvWorkBuffer;
	    }
	    else {
	      lpvOutBuffer = (LPVOID)((LPBYTE)lpvDestinationMem+1);
	    }

	    if ((LPBYTE)lpvOutBuffer+dwInSize > (LPBYTE)lpvInBuffer
	     && (LPBYTE)lpvInBuffer+dwInSize > (LPBYTE)lpvOutBuffer)
	    {
	      if ((LPBYTE)lpvOutBuffer+dwInSize > (LPBYTE)lpvWorkBuffer
	       && (LPBYTE)lpvWorkBuffer+dwInSize > (LPBYTE)lpvOutBuffer)
	      {
	        lpvWorkBuffer = lpvDestinationMem;
	        lpvOutBuffer = (LPVOID)((LPBYTE)lpvWorkBuffer+1);
	      }
	      else {
	        lpvOutBuffer = lpvWorkBuffer;
	      }
	    }

	    dwDecompressedSize = dwInSize-1;
	    lpCFuncTable->fnCompress(lpvOutBuffer,&dwDecompressedSize,lpvInBuffer,dwInSize,&dwCompressionSubType,dwCompressLevel);
	    dwOutSize = dwDecompressedSize+1;

	    if (dwOutSize < dwInSize) {
	      lpvInBuffer = lpvOutBuffer;
	      dwInSize = dwDecompressedSize;
	    }
	    else {
	      dwCurFlag = lpCFuncTable->dwFlag;
	      dwCompressionType = dwCompressionType & (~dwCurFlag);
	    }

	    lpvWorkBuffer = lpvAllocBuffer;
	  }

	  lpCFuncTable++;
	} while (lpCFuncTable <= lpLastCompressMethod);

	lpvOutBuffer = lpvDestinationMem;

	if (lpvInBuffer != lpvOutBuffer) {
	  if ((LPBYTE)lpvInBuffer == (LPBYTE)lpvOutBuffer+1) {
	    *(LPBYTE)lpvOutBuffer = (BYTE)dwCompressionType;
	    dwInSize++;
	  }
	  else {
	    if (dwCompressionType) {
	      SMemCopy((LPBYTE)lpvOutBuffer+1,lpvInBuffer,dwInSize);
	      dwInSize++;
	      *(LPBYTE)lpvOutBuffer = (BYTE)dwCompressionType;
	    }
	    else {
	      SMemCopy(lpvOutBuffer,lpvInBuffer,dwInSize);
	    }

	    lpvWorkBuffer = lpvAllocBuffer;
	  }
	}

	*lpdwCompressedSize = dwInSize;

	if (lpvWorkBuffer) {
	  SMemFree(lpvWorkBuffer);
	}

	return TRUE;
}

BOOL WINAPI SCompDecompress(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize)
{
	DWORD dwDecompressedSize, dwOutSize, dwCompressTypes, dwCurFlag, dwTempFlags,dwCompressCount;
	LPVOID lpvInBuffer,lpvOutBuffer,lpvWorkBuffer,lpvTempBuffer;
	DecompressFunc *lpCFuncTable;

	if (!lpdwDecompressedSize) {
	  SErrSetLastError(ERROR_INVALID_PARAMETER);
	  return FALSE;
	}

	dwDecompressedSize = *lpdwDecompressedSize;

	if (dwDecompressedSize < dwCompressedSize || !lpvDestinationMem || !lpvSourceMem || dwCompressedSize < 1) {
	  SErrSetLastError(ERROR_INVALID_PARAMETER);
	  return FALSE;
	}

	if (dwCompressedSize == dwDecompressedSize) {
	  if (lpvDestinationMem == lpvSourceMem)
	    return TRUE;

	  SMemCopy(lpvDestinationMem,lpvSourceMem,dwCompressedSize);
	  return TRUE;
	}

	dwCompressCount = 0;
	dwCompressTypes = *(LPBYTE)lpvSourceMem;
	lpvInBuffer = (LPVOID)((LPBYTE)lpvSourceMem+1);
	dwCompressedSize--;
	dwTempFlags = dwCompressTypes;
	lpCFuncTable = lpLastDecompressMethod;

	do {
	  dwCurFlag = lpCFuncTable->dwFlag;

	  if (dwCompressTypes & dwCurFlag) {
	    dwCompressCount++;
	  }

	  dwTempFlags &= ~dwCurFlag;
	  lpCFuncTable--;;
	} while (lpCFuncTable >= DecompressionFunctions);

	if (dwTempFlags) {
	  return FALSE;
	}

	lpvWorkBuffer = 0;

	if (dwCompressCount >= 2
	  || !((LPBYTE)lpvDestinationMem+dwCompressedSize <= (LPBYTE)lpvInBuffer
	    || (LPBYTE)lpvInBuffer+dwCompressedSize <= (LPBYTE)lpvDestinationMem
	    || !dwCompressCount))
	{
	  lpvWorkBuffer = SMemAlloc(dwDecompressedSize);

	  if (!lpvWorkBuffer) {
	    SErrSetLastError(ERROR_NOT_ENOUGH_MEMORY);
	    return FALSE;
	  }
	}

	lpCFuncTable = lpLastDecompressMethod;

	do {
	  if (dwCompressTypes & lpCFuncTable->dwFlag) {
	    lpvOutBuffer = lpvWorkBuffer;
	    dwCompressCount--;

	    if (!(dwCompressCount & 1)) {
	      lpvOutBuffer = lpvDestinationMem;
	    }

	    if ((LPBYTE)lpvOutBuffer+dwCompressedSize > (LPBYTE)lpvInBuffer
	     && (LPBYTE)lpvInBuffer+dwCompressedSize > (LPBYTE)lpvOutBuffer)
	    {
	      if ((LPBYTE)lpvOutBuffer+dwCompressedSize <= (LPBYTE)lpvWorkBuffer) {
	        lpvOutBuffer = lpvWorkBuffer;
	      }
	      else {
	        lpvTempBuffer = lpvOutBuffer;
	        lpvOutBuffer = lpvDestinationMem;

	        if ((LPBYTE)lpvWorkBuffer+dwCompressedSize <= (LPBYTE)lpvTempBuffer)
	          lpvOutBuffer = lpvWorkBuffer;
	      }
	    }

	    dwOutSize = dwDecompressedSize;
	    lpCFuncTable->fnDecompress(lpvOutBuffer,&dwOutSize,lpvInBuffer,dwCompressedSize);
	    dwCompressedSize = dwOutSize;
	    lpvInBuffer = lpvOutBuffer;
	  }

	  lpCFuncTable--;
	} while (lpCFuncTable >= DecompressionFunctions);

	if (lpvInBuffer != lpvDestinationMem) {
	  SMemCopy(lpvDestinationMem,lpvInBuffer,dwCompressedSize);
	}

	*lpdwDecompressedSize = dwCompressedSize;

	if (lpvWorkBuffer) {
	  SMemFree(lpvWorkBuffer);
	}

	return TRUE;
}

void __fastcall InitWaveCompress(DWORD dwCompressLevel, LPDWORD *lplpdwCompressionSubType, LPDWORD lpdwCompressionSubType)
{
	if (dwCompressLevel) {
	  if (dwCompressLevel <= 2) {
	    *(LPBYTE)lplpdwCompressionSubType = 4;
	    *lpdwCompressionSubType = 6;
	    return;
	  }
	  else if (dwCompressLevel == 3) {
	    *(LPBYTE)lplpdwCompressionSubType = 6;
	    *lpdwCompressionSubType = 8;
	    return;
	  }
	}

	*(LPBYTE)lplpdwCompressionSubType = 5;
	*lpdwCompressionSubType = 7;
}

void __fastcall CompressWaveMono(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel)
{
	InitWaveCompress(dwCompressLevel,&lpdwCompressionSubType,lpdwCompressionSubType);
	*lpdwCompressedSize = CompressWave((LPBYTE)lpvDestinationMem,*lpdwCompressedSize,(short *)lpvSourceMem,dwDecompressedSize,1,(unsigned int)lpdwCompressionSubType & 0xFF);
}

void __fastcall CompressWaveStereo(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel)
{
	InitWaveCompress(dwCompressLevel,&lpdwCompressionSubType,lpdwCompressionSubType);
	*lpdwCompressedSize = CompressWave((LPBYTE)lpvDestinationMem,*lpdwCompressedSize,(short *)lpvSourceMem,dwDecompressedSize,2,(unsigned int)lpdwCompressionSubType & 0xFF);
}

void __fastcall HuffmanCompress(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel)
{
	THuffmannTree *ht;                  // Huffmann tree for compression
	TOutputStream os;                   // Output stream
	THTreeItem * pItem;
	int nCount;

	ht = (THuffmannTree *)SMemAlloc(sizeof(THuffmannTree));

	if (!ht) {
	  SErrSetLastError(ERROR_NOT_ENOUGH_MEMORY);
	  return;
	}

	// Initialize output stream
	os.pbOutBuffer = (unsigned char *)lpvDestinationMem;
	os.dwOutSize   = *lpdwCompressedSize;
	os.pbOutPos    = (unsigned char *)lpvDestinationMem;
	os.dwBitBuff   = 0;
	os.nBits       = 0;

	// Clear links for all the items in the tree
	for(pItem = ht->items0008, nCount = 0x203; nCount != 0; nCount--, pItem++)
	  pItem->ClearItemLinks();

	ht->pItem3054 = NULL;
	ht->pItem3054 = PTR_PTR(&ht->pItem3054);
	ht->pItem3058 = PTR_NOT(&ht->pItem3054);

	ht->pItem305C = NULL;
	ht->pFirst    = PTR_PTR(&ht->pFirst);
	ht->pLast     = PTR_NOT(&ht->pFirst);

	ht->offs0004  = 1;
	ht->nItems    = 0;

	*lpdwCompressedSize = ht->DoCompression(&os, (unsigned char *)lpvSourceMem, dwDecompressedSize, *lpdwCompressionSubType);
/*
	// The following code is not necessary to run, because it has no
	// effect on the output data. It only clears the huffmann tree, but when
	// the tree is on the stack, who cares ?
	while(PTR_INT(ht.pLast) > 0)
	{
	  pItem = ht.pItem305C->Call1501DB70(ht.pLast);
	  pItem->RemoveItem();
	}

	for(pItem = ht.pFirst; PTR_INT(ht.pItem3058) > 0; pItem = ht.pItem3058)
	  pItem->RemoveItem();
	PTR_PTR(&ht.pItem3054)->RemoveItem();

	for(pItem = ht.items0008 + 0x203, nCount = 0x203; nCount != 0; nCount--)
	{
	  pItem--;
	  pItem->RemoveItem();
	  pItem->RemoveItem();
	}
*/
}

#ifdef USE_ZLIB

void __fastcall Deflate(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel)
{
	if (*lpdwCompressionSubType == 0) {
		switch (dwCompressLevel) {
			case 1:
				dwCompressLevel = Z_BEST_COMPRESSION;
				break;
			case 2:
				dwCompressLevel = Z_BEST_SPEED;
				break;
			default:
				dwCompressLevel = (DWORD)Z_DEFAULT_COMPRESSION;
		}
	}

	compress2((LPBYTE)lpvDestinationMem,(unsigned long *)lpdwCompressedSize,(LPBYTE)lpvSourceMem,dwDecompressedSize,dwCompressLevel);
	*lpdwCompressionSubType = 0;
}

#endif

void __fastcall Implode(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel)
{
	BUFFERINFO BufferInfo;
	unsigned int dwCompType, dwDictSize;
	LPVOID lpvWorkBuffer;

	lpvWorkBuffer = SMemAlloc(CMP_BUFFER_SIZE);

	if (!lpvWorkBuffer) {
	  SErrSetLastError(ERROR_NOT_ENOUGH_MEMORY);
	  return;
	}

	BufferInfo.lpvSourceMem = lpvSourceMem;
	BufferInfo.dwSrcStart = 0;
	BufferInfo.dwSrcLen = dwDecompressedSize;
	BufferInfo.lpvDestinationMem = lpvDestinationMem;
	BufferInfo.dwDestStart = 0;
	BufferInfo.dwDestLen = *lpdwCompressedSize;

	dwCompType = (*lpdwCompressionSubType==2)?CMP_ASCII:CMP_BINARY;

	if (dwDecompressedSize >= 0xC00) {
	  dwDictSize = 0x1000;
	}
	else if (dwDecompressedSize < 0x600) {
	  dwDictSize = 0x400;
	}
	else {
	  dwDictSize = 0x800;
	}

	implode(FillInput,FillOutput,(char *)lpvWorkBuffer,&BufferInfo,&dwCompType,&dwDictSize);
	*lpdwCompressedSize = BufferInfo.dwDestStart;
	*lpdwCompressionSubType = 0;

	SMemFree(lpvWorkBuffer);
}

#ifdef USE_BZIP2

void __fastcall CompressBZ2(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, LPDWORD lpdwCompressionSubType, DWORD dwCompressLevel)
{
	int nBlockSize;

	nBlockSize = 9;
	if (lpdwCompressionSubType)
		if (*lpdwCompressionSubType >= 1 && *lpdwCompressionSubType <= 9)
			nBlockSize = *lpdwCompressionSubType;

	BZ2_bzBuffToBuffCompress((char *)lpvDestinationMem, (unsigned int *)lpdwCompressedSize, (char *)lpvSourceMem, (unsigned int)dwDecompressedSize, nBlockSize, 0, 0);
	*lpdwCompressionSubType = 0;
}

#endif

void __fastcall DecompressWaveMono(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize)
{
	*lpdwDecompressedSize = DecompressWave((LPBYTE)lpvDestinationMem,*lpdwDecompressedSize,(LPBYTE)lpvSourceMem,dwCompressedSize,1);
}

void __fastcall DecompressWaveStereo(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize)
{
	*lpdwDecompressedSize = DecompressWave((LPBYTE)lpvDestinationMem,*lpdwDecompressedSize,(LPBYTE)lpvSourceMem,dwCompressedSize,2);
}

void __fastcall HuffmanDecompress(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize)
{
	THuffmannTree *ht;
	TInputStream  is;
	THTreeItem * pItem;
	unsigned int nCount;

	ht = (THuffmannTree *)SMemAlloc(sizeof(THuffmannTree));

	if (!ht) {
	  SErrSetLastError(ERROR_NOT_ENOUGH_MEMORY);
	  return;
	}

	// Initialize input stream
	is.pbInBuffer  = (unsigned char *)lpvSourceMem;
	is.dwBitBuff   = *(unsigned long *)lpvSourceMem;
	is.pbInBuffer    += sizeof(unsigned long);
	is.nBits       = 32;

	// Initialize the Huffman tree
	for(pItem  = ht->items0008, nCount = 0x203; nCount != 0; pItem++, nCount--)
	  pItem->ClearItemLinks();

	ht->pItem3050 = NULL;
	ht->pItem3054 = PTR_PTR(&ht->pItem3054);
	ht->pItem3058 = PTR_NOT(ht->pItem3054);

	ht->pItem305C = NULL;
	ht->pFirst    = PTR_PTR(&ht->pFirst);
	ht->pLast     = PTR_NOT(ht->pFirst);

	ht->offs0004  = 1;
	ht->nItems    = 0;

	// Clear all TQDecompress items
	for(nCount = 0; nCount < sizeof(ht->qd3474) / sizeof(TQDecompress); nCount++)
	  ht->qd3474[nCount].offs00 = 0;

	*lpdwDecompressedSize = ht->DoDecompression((unsigned char *)lpvDestinationMem, *lpdwDecompressedSize, &is);
/*
	// The following code is not necessary to run, because it has no
	// effect on the output data. It only clears the huffmann tree, but when
	// the tree is on the stack, who cares ?
	while(PTR_INT(ht.pLast) > 0)
	{
	  pItem = ht.pItem305C->Call1501DB70(ht.pLast);
	  pItem->RemoveItem();
	}

	for(pItem = ht.pFirst; PTR_INT(ht.pItem3058) > 0; pItem = ht.pItem3058)
	  pItem->RemoveItem();
	PTR_PTR(&ht.pItem3054)->RemoveItem();

	for(pItem = ht.items0008 + 0x203, nCount = 0x203; nCount != 0; nCount--)
	{
	  pItem--;
	  pItem->RemoveItem();
	  pItem->RemoveItem();
	}
*/

	SMemFree(ht);
}

#ifdef USE_ZLIB

void __fastcall Inflate(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize)
{
	uncompress((LPBYTE)lpvDestinationMem,(unsigned long *)lpdwDecompressedSize,(LPBYTE)lpvSourceMem,dwCompressedSize);
}

#endif

void __fastcall Explode(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize)
{
	BUFFERINFO BufferInfo;
	LPVOID lpvWorkBuffer;

	lpvWorkBuffer = SMemAlloc(EXP_BUFFER_SIZE);

	if (!lpvWorkBuffer) {
	  SErrSetLastError(ERROR_NOT_ENOUGH_MEMORY);
	  return;
	}

	BufferInfo.lpvSourceMem = lpvSourceMem;
	BufferInfo.dwSrcStart = 0;
	BufferInfo.dwSrcLen = dwCompressedSize;
	BufferInfo.lpvDestinationMem = lpvDestinationMem;
	BufferInfo.dwDestStart = 0;
	BufferInfo.dwDestLen = *lpdwDecompressedSize;

	explode(FillInput,FillOutput,(char *)lpvWorkBuffer,&BufferInfo);
	*lpdwDecompressedSize = BufferInfo.dwDestStart;

	SMemFree(lpvWorkBuffer);
}

#ifdef USE_BZIP2

void __fastcall DecompressBZ2(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize)
{
	BZ2_bzBuffToBuffDecompress((char *)lpvDestinationMem, (unsigned int *)lpdwDecompressedSize, (char *)lpvSourceMem, (unsigned int)dwCompressedSize, 0, 0);
}

#endif

unsigned int FillInput(char *lpvBuffer, unsigned int *lpdwSize, void *param)
{
	DWORD dwBufferSize;
	BUFFERINFO *lpBufferInfo = (BUFFERINFO *)param;

	dwBufferSize = *lpdwSize;

	if (dwBufferSize >= lpBufferInfo->dwSrcLen - lpBufferInfo->dwSrcStart) {
	  dwBufferSize = lpBufferInfo->dwSrcLen - lpBufferInfo->dwSrcStart;
	}

	SMemCopy(lpvBuffer,(LPBYTE)lpBufferInfo->lpvSourceMem+lpBufferInfo->dwSrcStart,dwBufferSize);
	lpBufferInfo->dwSrcStart += dwBufferSize;
	return dwBufferSize;
}

void FillOutput(char *lpvBuffer, unsigned int *lpdwSize, void *param)
{
	DWORD dwBufferSize;
	BUFFERINFO *lpBufferInfo = (BUFFERINFO *)param;

	dwBufferSize = *lpdwSize;

	if (dwBufferSize >= lpBufferInfo->dwDestLen - lpBufferInfo->dwDestStart) {
	  dwBufferSize = lpBufferInfo->dwDestLen - lpBufferInfo->dwDestStart;
	}

	SMemCopy((LPBYTE)lpBufferInfo->lpvDestinationMem+lpBufferInfo->dwDestStart,lpvBuffer,dwBufferSize);
	lpBufferInfo->dwDestStart += dwBufferSize;
}

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_BZIP2

void bz_internal_error ( int errcode )
{
}

#endif

#ifdef __cplusplus
};  // extern "C"
#endif
