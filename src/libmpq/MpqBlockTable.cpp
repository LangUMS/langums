// License information for this code is in license.txt

#include <windows.h>
#include "SFmpqapi.h"
#include "SFUtil.h"
#include "MpqCrypt.h"

BOOL WriteBlockTable(MPQARCHIVE *mpqOpenArc)
{
	DWORD tsz;

	if (mpqOpenArc->MpqHeader.dwBlockTableSize == 0) return TRUE;
	if (!mpqOpenArc->lpBlockTable) return FALSE;

	char *buffer = (char *)SFAlloc(sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize);
	if (buffer) {
		memcpy(buffer,mpqOpenArc->lpBlockTable,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize);
		EncryptData((LPBYTE)buffer,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize,dwBlockTableKey);
		SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->MpqHeader.dwBlockTableOffset,FILE_BEGIN);
		WriteFile(mpqOpenArc->hFile,buffer,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize,&tsz,0);
		SFFree(buffer);
	}
	else {
		EncryptData((LPBYTE)mpqOpenArc->lpBlockTable,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize,dwBlockTableKey);
		SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->MpqHeader.dwBlockTableOffset,FILE_BEGIN);
		WriteFile(mpqOpenArc->hFile,mpqOpenArc->lpBlockTable,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize,&tsz,0);
		DecryptData((LPBYTE)mpqOpenArc->lpBlockTable,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize,dwBlockTableKey);
	}

	return TRUE;
}
