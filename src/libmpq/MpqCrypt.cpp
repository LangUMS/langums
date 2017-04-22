// License information for this code is in license.txt

#include <string.h>
#include <ctype.h>
#include "MpqCrypt.h"
#include "SFTypes.h"

bool bCryptTableInit = false;
UInt32 dwCryptTable[0x500];
UInt32 dwHashTableKey;
UInt32 dwBlockTableKey;

// The InitCryptTable, HashString, DecryptData, and DetectFileKey are
// based on the versions in StormLib which were written by Ladislav 
// Zezula, but may have been modified somewhat by Quantam or ShadowFlare.
bool InitCryptTable()
{
	UInt32 seed   = 0x00100001;
	UInt32 index1 = 0;
	UInt32 index2 = 0;
	int   i;
		
	if (!bCryptTableInit)
	{
		 for(index1 = 0; index1 < 0x100; index1++)
		 {
			  for(index2 = index1, i = 0; i < 5; i++, index2 += 0x100)
			  {
					UInt32 temp1, temp2;
		
					seed  = (seed * 125 + 3) % 0x2AAAAB;
					temp1 = (seed & 0xFFFF) << 0x10;
		
					seed  = (seed * 125 + 3) % 0x2AAAAB;
					temp2 = (seed & 0xFFFF);
		
					dwCryptTable[index2] = (temp1 | temp2);
			  }
		 }

		bCryptTableInit = true;
	}

	return true;
}

UInt32 HashString(const char *lpszString, UInt32 dwHashType)
{
    UInt32  seed1 = 0x7FED7FED;
    UInt32  seed2 = 0xEEEEEEEE;
    int    ch;

	char szNull = 0;
	if (!lpszString)
		lpszString = &szNull;

	if (dwHashType==HASH_KEY)
		while (strchr(lpszString,'\\')!=NULL) lpszString = strchr(lpszString,'\\')+1;
    while (*lpszString != 0)
    {
        ch = toupper(*lpszString++);

        seed1 = dwCryptTable[(dwHashType << 8) + ch] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }

    return seed1;
}

// The EncryptData function is based on the DecryptData function by
// Ladislav Zezula, but adapted by Quantam to encrypt rather than decrypt.
bool EncryptData(UInt8 *lpbyBuffer, UInt32 dwLength, UInt32 dwKey)
{
    UInt32 *lpdwBuffer = (UInt32 *)lpbyBuffer;
    UInt32 seed = 0xEEEEEEEE;
    UInt32 ch;

	if (!lpbyBuffer)
		return false;

    // Round to DWORDs
    dwLength >>= 2;

    while(dwLength-- > 0)

    {
        seed += dwCryptTable[0x400 + (dwKey & 0xFF)];
        ch = *lpdwBuffer ^ (dwKey + seed);

        dwKey = ((~dwKey << 0x15) + 0x11111111) | (dwKey >> 0x0B);
        seed = *lpdwBuffer + seed + (seed << 5) + 3;

		*lpdwBuffer++ = ch;
    }

	 return true;
}

bool DecryptData(UInt8 *lpbyBuffer, UInt32 dwLength, UInt32 dwKey)
{
	UInt32 *lpdwBuffer = (UInt32 *)lpbyBuffer;
    UInt32 seed = 0xEEEEEEEE;
    UInt32 ch;

	if (!lpbyBuffer)
		return false;

    // Round to DWORDs
    dwLength >>= 2;

    while(dwLength-- > 0)
    {
        seed += dwCryptTable[0x400 + (dwKey & 0xFF)];
        ch = *lpdwBuffer ^ (dwKey + seed);

        dwKey = ((~dwKey << 0x15) + 0x11111111) | (dwKey >> 0x0B);
        seed = ch + seed + (seed << 5) + 3;

		*lpdwBuffer++ = ch;
    }

	 return true;
}

//-----------------------------------------------------------------------------
// Functions tries to get file decryption key. The trick comes from block
// positions which are stored at the begin of each compressed file. We know the
// file size, that means we know number of blocks that means we know the first
// DWORD value in block position. And if we know encrypted and decrypted value,
// we can find the decryption key !!!
//
// hf    - MPQ file handle
// block - DWORD array of block positions
// ch    - Decrypted value of the first block pos

UInt32 DetectFileSeed(const UInt32 * block, UInt32 decrypted, UInt32 blocksize)
{
    UInt32 saveSeed1;
    UInt32 temp = *block ^ decrypted;    // temp = seed1 + seed2
                                        // temp = seed1 + stormBuffer[0x400 + (seed1 & 0xFF)] + 0xEEEEEEEE
    temp -= 0xEEEEEEEE;                 // temp = seed1 + stormBuffer[0x400 + (seed1 & 0xFF)]
    

    for(int i = 0; i < 0x100; i++)      // Try all 256 possibilities
    {
        UInt32 seed1;
        UInt32 seed2 = 0xEEEEEEEE;
        UInt32 ch;

        // Try the first DWORD (We exactly know the value)
        seed1  = temp - dwCryptTable[0x400 + i];
        seed2 += dwCryptTable[0x400 + (seed1 & 0xFF)];
        ch     = block[0] ^ (seed1 + seed2);

        if(ch != decrypted)
            continue;

        saveSeed1 = seed1 + 1;

        // If OK, continue and test the second value. We don't know exactly the value,
        // but we know that the second one has a value less than or equal to the
		// size of the block position table plus the block size
        seed1  = ((~seed1 << 0x15) + 0x11111111) | (seed1 >> 0x0B);
        seed2  = ch + seed2 + (seed2 << 5) + 3;

        seed2 += dwCryptTable[0x400 + (seed1 & 0xFF)];
        ch     = block[1] ^ (seed1 + seed2);

        if(ch <= decrypted + blocksize)
            return saveSeed1;
    }
    return 0;
}
