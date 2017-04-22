// License information for this code is in license.txt

#include <windows.h>
#include "SFmpqapi.h"
#include "SFUtil.h"
#include "MpqCrypt.h"

BOOL WriteHashTable(MPQARCHIVE *mpqOpenArc)
{
	DWORD tsz;

	char *buffer = (char *)SFAlloc(sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize);
	if (buffer) {
		memcpy(buffer,mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize);
		EncryptData((LPBYTE)buffer,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,dwHashTableKey);
		SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->MpqHeader.dwHashTableOffset,FILE_BEGIN);
		WriteFile(mpqOpenArc->hFile,buffer,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,&tsz,0);
		SFFree(buffer);
	}
	else {
		EncryptData((LPBYTE)mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,dwHashTableKey);
		SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->MpqHeader.dwHashTableOffset,FILE_BEGIN);
		WriteFile(mpqOpenArc->hFile,mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,&tsz,0);
		DecryptData((LPBYTE)mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,dwHashTableKey);
	}

	return TRUE;
}
