// License information for this code is in license.txt

#ifndef MPQCRYPT_INCLUDED
#define MPQCRYPT_INCLUDED

#include "SFTypes.h"

#define HASH_POSITION 0
#define HASH_NAME_A 1
#define HASH_NAME_B 2
#define HASH_KEY 3

extern UInt32 dwHashTableKey;
extern UInt32 dwBlockTableKey;

bool InitCryptTable();
UInt32 HashString(const char *lpszString, UInt32 dwHashType);
bool EncryptData(UInt8 *lpbyBuffer, UInt32 dwLength, UInt32 dwKey);
bool DecryptData(UInt8 *lpbyBuffer, UInt32 dwLength, UInt32 dwKey);
UInt32 DetectFileSeed(const UInt32 * block, UInt32 decrypted, UInt32 blocksize);

#endif // #ifndef MPQCRYPT_INCLUDED

