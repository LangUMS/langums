/* License information for this code is in license.txt */

#define WIN32_LEAN_AND_MEAN

// Includes
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef _USRDLL
#include <shellapi.h>
#endif

#include "../SComp/SComp.h"

#include "MpqCrypt.h"
#include "MpqHashTable.h"
#include "MpqBlockTable.h"
#include "SFmpqapi.h"
#include "SFUtil.h"
#include "SFTypes.h"

struct SFMPQAPIMODULE {
    SFMPQAPIMODULE();
    ~SFMPQAPIMODULE();
} SFMpqApi;

LCID LocaleID = 0;
BOOL SFMpqInit = FALSE;
HINSTANCE hStorm = 0;
#ifdef _USRDLL
HINSTANCE hSFMpqInstance = 0;
#endif

#define SFMPQ_CURRENT_VERSION {1,0,8,1}

SFMPQVERSION SFMpqVersion = SFMPQ_CURRENT_VERSION;
LPCSTR SFMpqVersionString = "ShadowFlare MPQ API Library v1.08";
float MpqVersion = 2.00;

// Class to simulate _alloca for cross-compiler compatibility
class TempAlloc {
public:
    TempAlloc();
    ~TempAlloc();
    TempAlloc(unsigned long int AllocSize);
    void * Alloc(unsigned long int AllocSize);
    void *lpAllocAddress;
};

LPCSTR ID_MPQ = "MPQ\x1A";
LPCSTR ID_BN3 = "BN3\x1A";
LPCSTR ID_RIFF = "RIFF";
LPCSTR ID_WAVE = "WAVEfmt ";
LPCSTR INTERNAL_LISTFILE = "(listfile)";
LPCSTR INTERNAL_FILES = "(attributes)\n(listfile)\n(signature)";
LPCSTR Storm_dll = "Storm.dll";
LPCSTR UNKNOWN_OUT = "~unknowns\\unknown_%08x";
LPCSTR UNKNOWN_IN = "~unknowns\\unknown_%x";
LPCSTR UNKNOWN_CMP = "~unknowns\\unknown_";
#define UNKNOWN_LEN 18

LCID availLocales[7] = {0x0000,0x0407,0x0409,0x040A,0x040C,0x0410,0x0416};
#define nLocales 7

#define MAX_MPQ_PATH 260;

MPQARCHIVE **lpOpenMpq = 0;
DWORD dwOpenMpqCount = 0;
MPQARCHIVE * FirstLastMpq[2] = {0,0};
MPQFILE **lpOpenFile = 0;
DWORD dwOpenFileCount = 0;
MPQFILE * FirstLastFile[2] = {0,0};

char StormBasePath[MAX_PATH+1];

typedef BOOL (WINAPI* funcSCompCompress)(LPVOID lpvDestinationMem, LPDWORD lpdwCompressedSize, LPVOID lpvSourceMem, DWORD dwDecompressedSize, DWORD dwCompressionType, DWORD dwCompressionSubType, DWORD dwWAVQuality);
typedef BOOL (WINAPI* funcSCompDecompress)(LPVOID lpvDestinationMem, LPDWORD lpdwDecompressedSize, LPVOID lpvSourceMem, DWORD dwCompressedSize);
funcSCompCompress stormSCompCompress = 0;
funcSCompDecompress stormSCompDecompress = 0;

void LoadStorm();
void FreeStorm();

BOOL WINAPI MpqOpenArchiveEx(LPCSTR lpFileName, DWORD dwPriority, DWORD dwFlags, MPQHANDLE *hMPQ, DWORD dwFlags2, DWORD dwMaximumFilesInArchive, DWORD dwBlockSize);
DWORD WINAPI FindMpqHeaderAtLocation(HANDLE hFile, DWORD dwStart, DWORD dwLength);
DWORD GetFullPath(LPCSTR lpFileName, char *lpBuffer, DWORD dwBufferLength);
MPQHANDLE GetHashTableEntry(MPQHANDLE hMPQ, LPCSTR lpFileName, LCID FileLocale);
MPQHANDLE GetHashTableEntryOfHash(MPQHANDLE hMPQ, DWORD dwTablePos, DWORD dwNameHashA, DWORD dwNameHashB, LCID FileLocale);
MPQHANDLE GetFreeHashTableEntry(MPQHANDLE hMPQ, LPCSTR lpFileName, LCID FileLocale, BOOL ReturnExisting);
BOOL SearchOpenArchives(LPCSTR lpFileName, MPQHANDLE *hMPQ, MPQHANDLE *hFile);
void SortOpenArchivesByPriority();
DWORD GetHandleType(MPQHANDLE hFile);
BOOL AddToInternalListing(MPQHANDLE hMPQ, LPCSTR lpFileName);
BOOL RemoveFromInternalListing(MPQHANDLE hMPQ, LPCSTR lpFileName);
DWORD DetectFileSeedEx(MPQARCHIVE * mpqOpenArc, HASHTABLEENTRY * lpHashEntry, LPSTR * lplpFileName);

extern "C" BOOL APIENTRY SFMpqMain( HINSTANCE hInstDLL, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
                     )
{
#ifdef _WIN32
    char *lpExeName,*buffer;DWORD slen;
#endif
    DWORD dwLastCount;

#ifdef _USRDLL
    hSFMpqInstance = hInstDLL;
#endif

    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            InitCryptTable();
            dwHashTableKey = HashString("(hash table)",HASH_KEY);
            dwBlockTableKey = HashString("(block table)",HASH_KEY);
#ifdef _WIN32
            lpExeName = (char *)SFAlloc(MAX_PATH+1);
            if (lpExeName) {
                slen = GetModuleFileName(0,lpExeName,MAX_PATH);
                lpExeName[slen] = 0;
                buffer = lpExeName+strlen(lpExeName);
                while (*buffer!='\\') {*buffer = 0;buffer--;}
                SFileSetBasePath(lpExeName);
                SFFree(lpExeName);
            }
#else
            SFileSetBasePath("./");
#endif
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            if (lpOpenFile) {
                while (dwOpenFileCount>0) {
                    dwLastCount = dwOpenFileCount;
                    SFileCloseFile((MPQHANDLE)lpOpenFile[dwOpenFileCount-1]);
                    if (dwLastCount==dwOpenFileCount) dwOpenFileCount--;
                }

                SFFree(lpOpenFile);
            }

            if (lpOpenMpq) {
                while (dwOpenMpqCount>0) {
                    dwLastCount = dwOpenMpqCount;
                    SFileCloseArchive((MPQHANDLE)lpOpenMpq[dwOpenMpqCount-1]);
                    if (dwLastCount==dwOpenMpqCount) dwOpenMpqCount--;
                }

                SFFree(lpOpenMpq);
            }
            break;
    }

    return TRUE;
}

TempAlloc::TempAlloc()
{
    lpAllocAddress = 0;
}

TempAlloc::TempAlloc(unsigned long int AllocSize)
{
    lpAllocAddress = SFAlloc(AllocSize);
}

TempAlloc::~TempAlloc()
{
    if (lpAllocAddress) {
        SFFree(lpAllocAddress);
        lpAllocAddress = 0;
    }
}

void * TempAlloc::Alloc(unsigned long int AllocSize)
{
    lpAllocAddress = SFAlloc(AllocSize);
    return lpAllocAddress;
}

SFMPQAPIMODULE::SFMPQAPIMODULE()
{
    //LoadStorm();
}

SFMPQAPIMODULE::~SFMPQAPIMODULE()
{
    FreeStorm();
}

BOOL SFMPQAPI WINAPI SFileDestroy() {
    return TRUE;
}

void SFMPQAPI WINAPI StormDestroy() {
}

void SFMPQAPI WINAPI SFMpqDestroy() {
    //FreeStorm();
}

BOOL SFMPQAPI WINAPI MpqInitialize()
{
     return (SFMpqInit=TRUE);
}

LPCSTR SFMPQAPI WINAPI MpqGetVersionString()
{
    return SFMpqVersionString;
}

float SFMPQAPI WINAPI MpqGetVersion()
{
    return MpqVersion;
}

LPCSTR SFMPQAPI WINAPI SFMpqGetVersionString()
{
    return SFMpqVersionString;
}

DWORD  SFMPQAPI WINAPI SFMpqGetVersionString2(LPSTR lpBuffer, DWORD dwBufferLength)
{
    if (!lpBuffer) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return strlen(SFMpqVersionString)+1;
    }

    DWORD slen = strlen(SFMpqVersionString)+1;

    if (dwBufferLength>=slen) memcpy(lpBuffer,SFMpqVersionString,slen);
    else memcpy(lpBuffer,SFMpqVersionString,dwBufferLength);

    return slen;
}

SFMPQVERSION SFMPQAPI WINAPI SFMpqGetVersion()
{
    return SFMpqVersion;
}

void SFMPQAPI WINAPI AboutSFMpq()
{
#ifdef _USRDLL
    if (hSFMpqInstance) {
        char szAboutPage[MAX_PATH+13];
        strcpy(szAboutPage,"res://");
        GetModuleFileName(hSFMpqInstance,szAboutPage+6,MAX_PATH);
        strcat(szAboutPage,"/about");
        ShellExecute(0,0,szAboutPage,0,0,SW_SHOWNORMAL);
    }
#endif
}

BOOL WINAPI MpqOpenArchiveEx(LPCSTR lpFileName, DWORD dwPriority, DWORD dwFlags, MPQHANDLE *hMPQ, DWORD dwFlags2, DWORD dwMaximumFilesInArchive, DWORD dwBlockSize)
{
    DWORD flen,tsz;

    if (!lpFileName || !hMPQ) {
        SetLastError(ERROR_INVALID_PARAMETER);
        if (hMPQ) *hMPQ = 0;
        return FALSE;
    }
    if (!*lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        *hMPQ = 0;
        return FALSE;
    }

    DWORD dwFlags1 = 0;

#ifdef _WIN32
    char RootPath[4];
    memset(RootPath,0,4);
    if (memcmp(lpFileName+1,":\\",2)==0) memcpy(RootPath,lpFileName,3);
    else {
        char CurDir[MAX_PATH];
        GetCurrentDirectory(MAX_PATH,CurDir);
        memcpy(RootPath,CurDir,3);
    }
    if (GetDriveType(RootPath)==DRIVE_CDROM)
        dwFlags1 |= 2;
    else if (dwFlags&2)
        dwFlags1 |= 1;
    if (dwFlags & SFILE_OPEN_CD_ROM_FILE)
        if (!(dwFlags1&2)) {*hMPQ = 0;return FALSE;}
#endif

    DWORD dwCreateFlags,dwAccessFlags,dwShareFlags;
    if (dwFlags2 & MOAU_OPEN_ALWAYS) dwCreateFlags = OPEN_ALWAYS;
    else if (dwFlags2 & MOAU_CREATE_ALWAYS) dwCreateFlags = CREATE_ALWAYS;
    else if (dwFlags2 & MOAU_OPEN_EXISTING) dwCreateFlags = OPEN_EXISTING;
    else dwCreateFlags = CREATE_NEW;
    if (dwFlags2 & MOAU_READ_ONLY) {
        if (!(dwFlags2 & MOAU_OPEN_EXISTING)) {
            SetLastError(MPQ_ERROR_BAD_OPEN_MODE);
            *hMPQ = 0;
            return FALSE;
        }
        dwAccessFlags = GENERIC_READ;
        if (dwFlags2 & SFILE_OPEN_ALLOW_WRITE) dwFlags2 = dwFlags2^SFILE_OPEN_ALLOW_WRITE;
    }
    else {
        dwAccessFlags = GENERIC_READ|GENERIC_WRITE;
        dwFlags2 = dwFlags2|SFILE_OPEN_ALLOW_WRITE;
    }
    if (dwAccessFlags & GENERIC_WRITE)
        dwShareFlags = 0;
    else
        dwShareFlags = FILE_SHARE_READ;
    HANDLE hFile;
    hFile = CreateFile(lpFileName,dwAccessFlags,dwShareFlags,0,dwCreateFlags,0,0);
    TempAlloc NewAlloc;
    if (hFile==INVALID_HANDLE_VALUE) {
        DWORD namelen = GetFullPath(lpFileName,0,0);
        char *namebuffer = (char *)NewAlloc.Alloc(namelen);
        GetFullPath(lpFileName,namebuffer,namelen);
        lpFileName = namebuffer;
        hFile = CreateFile(lpFileName,dwAccessFlags,dwShareFlags,0,dwCreateFlags,0,0);
    }
    if (hFile!=INVALID_HANDLE_VALUE)
    {
        MPQARCHIVE **lpnOpenMpq = (MPQARCHIVE **)SFAlloc(sizeof(MPQARCHIVE *) * (dwOpenMpqCount+1));
        if (lpnOpenMpq==0) {
            CloseHandle(hFile);
            *hMPQ = 0;
            return FALSE;
        }
        DWORD dwMpqStart;
        MPQARCHIVE *mpqOpenArc;
        if (SFGetFileSize(hFile)==0 && !(dwFlags2 & MOAU_READ_ONLY))
        {
            dwMpqStart = 0;
            mpqOpenArc = (MPQARCHIVE *)SFAlloc(sizeof(MPQARCHIVE));
            if (!mpqOpenArc) {
                SFFree(lpnOpenMpq);
                CloseHandle(hFile);
                *hMPQ = 0;
                return FALSE;
            }
            memcpy(&mpqOpenArc->MpqHeader.dwMPQID,ID_MPQ,4);
            mpqOpenArc->MpqHeader.dwHeaderSize = sizeof(MPQHEADER);
            mpqOpenArc->MpqHeader.wUnused0C = 0;
            if (dwBlockSize & 0xFFFF0000)
                mpqOpenArc->MpqHeader.wBlockSize = DEFAULT_BLOCK_SIZE;
            else
                mpqOpenArc->MpqHeader.wBlockSize = (WORD)dwBlockSize;
            DWORD i;
            for (i=1;i<dwMaximumFilesInArchive;i*=2) {}
            dwMaximumFilesInArchive = i;
            mpqOpenArc->MpqHeader.dwHashTableSize = dwMaximumFilesInArchive;
            mpqOpenArc->MpqHeader.dwBlockTableSize = 0;
            mpqOpenArc->MpqHeader.dwHashTableOffset = mpqOpenArc->MpqHeader.dwHeaderSize;
            mpqOpenArc->MpqHeader.dwBlockTableOffset = mpqOpenArc->MpqHeader.dwHeaderSize + mpqOpenArc->MpqHeader.dwHashTableSize * sizeof(HASHTABLEENTRY);
            mpqOpenArc->MpqHeader.dwMPQSize = mpqOpenArc->MpqHeader.dwHeaderSize + mpqOpenArc->MpqHeader.dwHashTableSize * sizeof(HASHTABLEENTRY);
            if(WriteFile(hFile,&mpqOpenArc->MpqHeader,mpqOpenArc->MpqHeader.dwHeaderSize,&tsz,0)==0) {
                SFFree(lpnOpenMpq);
                SFFree(mpqOpenArc);
                CloseHandle(hFile);
                *hMPQ = 0;
                return FALSE;
            }
            flen = mpqOpenArc->MpqHeader.dwMPQSize;
            mpqOpenArc->lpHashTable = (HASHTABLEENTRY *)SFAlloc(sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize);
            if(!mpqOpenArc->lpHashTable) {
                SFFree(lpnOpenMpq);
                SFFree(mpqOpenArc);
                CloseHandle(hFile);
                *hMPQ = 0;
                return FALSE;
            }
            memset(mpqOpenArc->lpHashTable,0xFF,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize);
            EncryptData((LPBYTE)mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,dwHashTableKey);
            if(WriteFile(hFile,mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,&tsz,0)==0) {
                SFFree(mpqOpenArc->lpHashTable);
                SFFree(lpnOpenMpq);
                SFFree(mpqOpenArc);
                CloseHandle(hFile);
                *hMPQ = 0;
                return FALSE;
            }
        }
        else
        {
            dwMpqStart = SFileFindMpqHeader(hFile);
            if (dwMpqStart==0xFFFFFFFF) {
                SFFree(lpnOpenMpq);
                CloseHandle(hFile);
                SetLastError(MPQ_ERROR_MPQ_INVALID);
                *hMPQ = 0;
                return FALSE;
            }
            flen = (DWORD)SFGetFileSize(hFile);
            mpqOpenArc = (MPQARCHIVE *)SFAlloc(sizeof(MPQARCHIVE));
            if (!mpqOpenArc) {
                SFFree(lpnOpenMpq);
                CloseHandle(hFile);
                *hMPQ = 0;
                return FALSE;
            }
            SFSetFilePointer(hFile,dwMpqStart,FILE_BEGIN);
            if(ReadFile(hFile,&mpqOpenArc->MpqHeader,sizeof(MPQHEADER),&tsz,0)==0) {
                SFFree(lpnOpenMpq);
                SFFree(mpqOpenArc);
                CloseHandle(hFile);
                *hMPQ = 0;
                return FALSE;
            }
            mpqOpenArc->lpHashTable = (HASHTABLEENTRY *)SFAlloc(sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize);
            if(!mpqOpenArc->lpHashTable) {
                SFFree(lpnOpenMpq);
                SFFree(mpqOpenArc);
                CloseHandle(hFile);
                *hMPQ = 0;
                return FALSE;
            }
            if (mpqOpenArc->MpqHeader.dwBlockTableSize!=0) {
                mpqOpenArc->lpBlockTable = (BLOCKTABLEENTRY *)SFAlloc(sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize);
                if(!mpqOpenArc->lpBlockTable) {
                    SFFree(mpqOpenArc->lpHashTable);
                    SFFree(lpnOpenMpq);
                    SFFree(mpqOpenArc);
                    CloseHandle(hFile);
                    *hMPQ = 0;
                    return FALSE;
                }
            }
            SFSetFilePointer(hFile,dwMpqStart+mpqOpenArc->MpqHeader.dwHashTableOffset,FILE_BEGIN);
            if(ReadFile(hFile,mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,&tsz,0)==0) {
                if(mpqOpenArc->lpBlockTable) SFFree(mpqOpenArc->lpBlockTable);
                SFFree(mpqOpenArc->lpHashTable);
                SFFree(lpnOpenMpq);
                SFFree(mpqOpenArc);
                CloseHandle(hFile);
                *hMPQ = 0;
                return FALSE;
            }
            if (mpqOpenArc->MpqHeader.dwBlockTableSize!=0) {
                SFSetFilePointer(hFile,dwMpqStart+mpqOpenArc->MpqHeader.dwBlockTableOffset,FILE_BEGIN);
                if(ReadFile(hFile,mpqOpenArc->lpBlockTable,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize,&tsz,0)==0) {
                    SFFree(mpqOpenArc->lpBlockTable);
                    SFFree(mpqOpenArc->lpHashTable);
                    SFFree(lpnOpenMpq);
                    SFFree(mpqOpenArc);
                    CloseHandle(hFile);
                    *hMPQ = 0;
                    return FALSE;
                }
            }
        }
        /*mpqOpenArc->lpFileName = (char *)SFAlloc(strlen(lpFileName)+1);
        if(!mpqOpenArc->lpFileName) {
            if(mpqOpenArc->lpBlockTable) SFFree(mpqOpenArc->lpBlockTable);
            SFFree(mpqOpenArc->lpHashTable);
            SFFree(lpnOpenMpq);
            SFFree(mpqOpenArc);
            CloseHandle(hFile);
            *hMPQ = 0;
            return FALSE;
        }*/
        DecryptData((LPBYTE)mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,dwHashTableKey);
        if (mpqOpenArc->lpBlockTable) DecryptData((LPBYTE)mpqOpenArc->lpBlockTable,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize,dwBlockTableKey);
        mpqOpenArc->lpFileName = mpqOpenArc->szFileName;
        strncpy(mpqOpenArc->lpFileName,lpFileName,259);
        if (FirstLastMpq[1]) FirstLastMpq[1]->lpNextArc = mpqOpenArc;
        mpqOpenArc->lpNextArc = (MPQARCHIVE *)FirstLastMpq;
        mpqOpenArc->lpPrevArc = (MPQARCHIVE *)FirstLastMpq[1];
        if (!FirstLastMpq[0]) {
            mpqOpenArc->lpPrevArc = (MPQARCHIVE *)0xEAFC5E23;
            FirstLastMpq[0] = mpqOpenArc;
        }
        FirstLastMpq[1] = mpqOpenArc;
        mpqOpenArc->hFile = hFile;
        mpqOpenArc->dwFlags1 = dwFlags1;
        mpqOpenArc->dwPriority = dwPriority;
        mpqOpenArc->lpLastReadFile = 0;
        mpqOpenArc->dwUnk = 0;
        mpqOpenArc->dwBlockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
        mpqOpenArc->lpLastReadBlock = 0;
        mpqOpenArc->dwBufferSize = 0;
        mpqOpenArc->dwMPQStart = dwMpqStart;
        mpqOpenArc->lpMPQHeader = &mpqOpenArc->MpqHeader;
        mpqOpenArc->dwReadOffset = flen;
        mpqOpenArc->dwRefCount = 1;
        mpqOpenArc->dwFlags = dwFlags2;
        mpqOpenArc->dwExtraFlags = 0;
        memcpy(lpnOpenMpq,lpOpenMpq,sizeof(MPQARCHIVE *) * dwOpenMpqCount);
        lpnOpenMpq[dwOpenMpqCount] = mpqOpenArc;
        if (lpOpenMpq) SFFree(lpOpenMpq);
        lpOpenMpq = lpnOpenMpq;
        dwOpenMpqCount++;
        if (dwOpenMpqCount>1) SortOpenArchivesByPriority();
        *hMPQ = (MPQHANDLE)mpqOpenArc;
        return TRUE;
    }
    else {
        if (dwFlags2&MOAU_OPEN_EXISTING) SetLastError(ERROR_FILE_NOT_FOUND);
        else if (dwFlags2&MOAU_CREATE_NEW) SetLastError(ERROR_ALREADY_EXISTS);
        *hMPQ = (MPQHANDLE)INVALID_HANDLE_VALUE;
        return FALSE;
    }
}

BOOL SFMPQAPI WINAPI SFileOpenFileAsArchive(MPQHANDLE hSourceMPQ, LPCSTR lpFileName, DWORD dwPriority, DWORD dwFlags, MPQHANDLE *hMPQ)
{
    DWORD flen,tsz;

    if (!lpFileName || !hMPQ) {
        SetLastError(ERROR_INVALID_PARAMETER);
        if (hMPQ) *hMPQ = 0;
        return FALSE;
    }
    if (!*lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        *hMPQ = 0;
        return FALSE;
    }

    HANDLE hFile;
    if (SFileOpenFileEx(hSourceMPQ,lpFileName,1,&hFile))
    {
        MPQFILE mpqArcFile;
        mpqArcFile = *(MPQFILE *)hFile;
        SFileCloseFile(hFile);
        if (mpqArcFile.hFile != INVALID_HANDLE_VALUE)
            return SFileOpenArchive(lpFileName,dwPriority,dwFlags,hMPQ);
        hFile = mpqArcFile.lpParentArc->hFile;
        MPQARCHIVE **lpnOpenMpq = (MPQARCHIVE **)SFAlloc(sizeof(MPQARCHIVE *) * (dwOpenMpqCount+1));
        if (!lpnOpenMpq) {
            *hMPQ = 0;
            return FALSE;
        }
        DWORD dwMpqStart;
        MPQARCHIVE *mpqOpenArc;
        dwMpqStart = mpqArcFile.lpParentArc->dwMPQStart + mpqArcFile.lpBlockEntry->dwFileOffset;
        flen = mpqArcFile.lpBlockEntry->dwFullSize;
        dwMpqStart = FindMpqHeaderAtLocation(hFile,dwMpqStart,flen);
        if (dwMpqStart==0xFFFFFFFF) {
            SFFree(lpnOpenMpq);
            SetLastError(MPQ_ERROR_MPQ_INVALID);
            *hMPQ = 0;
            return FALSE;
        }
        mpqOpenArc = (MPQARCHIVE *)SFAlloc(sizeof(MPQARCHIVE));
        if (!mpqOpenArc) {
            SFFree(lpnOpenMpq);
            *hMPQ = 0;
            return FALSE;
        }
        SFSetFilePointer(hFile,dwMpqStart,FILE_BEGIN);
        if(ReadFile(hFile,&mpqOpenArc->MpqHeader,sizeof(MPQHEADER),&tsz,0)==0) {
            SFFree(lpnOpenMpq);
            SFFree(mpqOpenArc);
            *hMPQ = 0;
            return FALSE;
        }
        mpqOpenArc->lpHashTable = (HASHTABLEENTRY *)SFAlloc(sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize);
        if(!mpqOpenArc->lpHashTable) {
            SFFree(lpnOpenMpq);
            SFFree(mpqOpenArc);
            *hMPQ = 0;
            return FALSE;
        }
        if (mpqOpenArc->MpqHeader.dwBlockTableSize!=0) {
            mpqOpenArc->lpBlockTable = (BLOCKTABLEENTRY *)SFAlloc(sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize);
            if(!mpqOpenArc->lpBlockTable) {
                SFFree(mpqOpenArc->lpHashTable);
                SFFree(lpnOpenMpq);
                SFFree(mpqOpenArc);
                *hMPQ = 0;
                return FALSE;
            }
        }
        SFSetFilePointer(hFile,dwMpqStart+mpqOpenArc->MpqHeader.dwHashTableOffset,FILE_BEGIN);
        if(ReadFile(hFile,mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,&tsz,0)==0) {
            if(mpqOpenArc->lpBlockTable) SFFree(mpqOpenArc->lpBlockTable);
            SFFree(mpqOpenArc->lpHashTable);
            SFFree(lpnOpenMpq);
            SFFree(mpqOpenArc);
            *hMPQ = 0;
            return FALSE;
        }
        if (mpqOpenArc->MpqHeader.dwBlockTableSize!=0) {
            SFSetFilePointer(hFile,dwMpqStart+mpqOpenArc->MpqHeader.dwBlockTableOffset,FILE_BEGIN);
            if(ReadFile(hFile,mpqOpenArc->lpBlockTable,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize,&tsz,0)==0) {
                SFFree(mpqOpenArc->lpBlockTable);
                SFFree(mpqOpenArc->lpHashTable);
                SFFree(lpnOpenMpq);
                SFFree(mpqOpenArc);
                *hMPQ = 0;
                return FALSE;
            }
        }
        /*mpqOpenArc->lpFileName = (char *)SFAlloc(strlen(lpFileName)+1);
        if(!mpqOpenArc->lpFileName) {
            if(mpqOpenArc->lpBlockTable) SFFree(mpqOpenArc->lpBlockTable);
            SFFree(mpqOpenArc->lpHashTable);
            SFFree(lpnOpenMpq);
            SFFree(mpqOpenArc);
            *hMPQ = 0;
            return FALSE;
        }*/
        DecryptData((LPBYTE)mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize,dwHashTableKey);
        if (mpqOpenArc->lpBlockTable) DecryptData((LPBYTE)mpqOpenArc->lpBlockTable,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize,dwBlockTableKey);
        mpqOpenArc->lpFileName = mpqOpenArc->szFileName;
        strncpy(mpqOpenArc->lpFileName,lpFileName,259);
        if (FirstLastMpq[1]) FirstLastMpq[1]->lpNextArc = mpqOpenArc;
        mpqOpenArc->lpNextArc = (MPQARCHIVE *)FirstLastMpq;
        mpqOpenArc->lpPrevArc = (MPQARCHIVE *)FirstLastMpq[1];
        if (!FirstLastMpq[0]) {
            mpqOpenArc->lpPrevArc = (MPQARCHIVE *)0xEAFC5E23;
            FirstLastMpq[0] = mpqOpenArc;
        }
        FirstLastMpq[1] = mpqOpenArc;
        mpqOpenArc->hFile = hFile;
        mpqOpenArc->dwFlags1 = 0;
        mpqOpenArc->dwPriority = dwPriority;
        mpqOpenArc->lpLastReadFile = 0;
        mpqOpenArc->dwUnk = 0;
        mpqOpenArc->dwBlockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
        mpqOpenArc->lpLastReadBlock = 0;
        mpqOpenArc->dwBufferSize = 0;
        mpqOpenArc->dwMPQStart = dwMpqStart;
        mpqOpenArc->lpMPQHeader = &mpqOpenArc->MpqHeader;
        mpqOpenArc->dwReadOffset = flen;
        mpqOpenArc->dwRefCount = 1;
        mpqOpenArc->dwFlags = dwFlags;
        mpqOpenArc->dwExtraFlags = 1;
        memcpy(lpnOpenMpq,lpOpenMpq,sizeof(MPQARCHIVE *) * dwOpenMpqCount);
        lpnOpenMpq[dwOpenMpqCount] = mpqOpenArc;
        if (lpOpenMpq) SFFree(lpOpenMpq);
        lpOpenMpq = lpnOpenMpq;
        dwOpenMpqCount++;
        if (dwOpenMpqCount>1) SortOpenArchivesByPriority();
        *hMPQ = (MPQHANDLE)mpqOpenArc;
        return TRUE;
    }
    else {
        *hMPQ = (MPQHANDLE)INVALID_HANDLE_VALUE;
        return FALSE;
    }
}

BOOL SFMPQAPI WINAPI SFileOpenArchive(LPCSTR lpFileName, DWORD dwPriority, DWORD dwFlags, MPQHANDLE *hMPQ)
{
    if (dwFlags & SFILE_OPEN_ALLOW_WRITE) {
        return MpqOpenArchiveEx(lpFileName,dwPriority,dwFlags ^ SFILE_OPEN_ALLOW_WRITE,hMPQ,MOAU_OPEN_EXISTING,0,USE_DEFAULT_BLOCK_SIZE);
    }
    else {
        return MpqOpenArchiveEx(lpFileName,dwPriority,dwFlags,hMPQ,MOAU_OPEN_EXISTING|MOAU_READ_ONLY,0,USE_DEFAULT_BLOCK_SIZE);
    }
}

BOOL SFMPQAPI WINAPI SFileCloseArchive(MPQHANDLE hMPQ)
{
    if (hMPQ==0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    if (dwOpenMpqCount==0) return FALSE;
    if (memcmp(&mpqOpenArc->MpqHeader.dwMPQID,ID_MPQ,4)!=0 && memcmp(&mpqOpenArc->MpqHeader.dwMPQID,ID_BN3,4)!=0) return FALSE;
    if (!(mpqOpenArc->dwExtraFlags & 1)) CloseHandle(mpqOpenArc->hFile);
    //SFFree(mpqOpenArc->lpFileName);
    SFFree(mpqOpenArc->lpHashTable);
    if(mpqOpenArc->lpBlockTable) SFFree(mpqOpenArc->lpBlockTable);
    if ((DWORD)mpqOpenArc->lpNextArc!=(DWORD)FirstLastMpq)
        mpqOpenArc->lpNextArc->lpPrevArc = mpqOpenArc->lpPrevArc;
    if ((DWORD)mpqOpenArc->lpPrevArc!=0xEAFC5E23)
        mpqOpenArc->lpPrevArc->lpNextArc = mpqOpenArc->lpNextArc;
    if (mpqOpenArc==FirstLastMpq[0])
        FirstLastMpq[0] = mpqOpenArc->lpNextArc;
    if ((DWORD)FirstLastMpq[0]==(DWORD)FirstLastMpq)
        FirstLastMpq[0] = 0;
    if (mpqOpenArc==FirstLastMpq[1])
        FirstLastMpq[1] = mpqOpenArc->lpPrevArc;
    if ((DWORD)FirstLastMpq[1]==0xEAFC5E23)
        FirstLastMpq[1] = 0;
    SFFree(mpqOpenArc);
    if (!lpOpenMpq) return TRUE;
    if (dwOpenMpqCount-1==0) {
        SFFree(lpOpenMpq);
        lpOpenMpq = 0;
        dwOpenMpqCount--;
        return TRUE;
    }
    else if (dwOpenMpqCount==0) return FALSE;
    MPQARCHIVE **lpnOpenMpq = (MPQARCHIVE **)SFAlloc(sizeof(MPQARCHIVE *) * (dwOpenMpqCount-1));
    if (lpnOpenMpq) {
        for (DWORD i=0,j=0;i<dwOpenMpqCount;i++) {
            if ((MPQHANDLE)lpOpenMpq[i]!=hMPQ) {
                memcpy(&lpnOpenMpq[j],&lpOpenMpq[i],sizeof(MPQARCHIVE *));
                j++;
            }
        }
        SFFree(lpOpenMpq);
        lpOpenMpq = lpnOpenMpq;
        dwOpenMpqCount--;
    }
    else {
        for (DWORD i=0,j=0;i<dwOpenMpqCount;i++) {
            if ((MPQHANDLE)lpOpenMpq[i]==hMPQ) {
                lpOpenMpq[i] = (MPQARCHIVE *)0;
            }
            else {
                if (i!=j) memcpy(&lpOpenMpq[j],&lpOpenMpq[i],sizeof(MPQARCHIVE *));
                j++;
            }
        }
        dwOpenMpqCount--;
    }
    return TRUE;
}

BOOL SFMPQAPI WINAPI SFileGetArchiveName(MPQHANDLE hMPQ, LPSTR lpBuffer, DWORD dwBufferLength)
{
    if (hMPQ==0 || lpBuffer==0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    DWORD dwhType = GetHandleType(hMPQ);
    const char *lpFileName;
    if (dwhType==SFILE_TYPE_MPQ) lpFileName = ((MPQARCHIVE *)hMPQ)->lpFileName;
    else if (dwhType==SFILE_TYPE_FILE) lpFileName = ((MPQFILE *)hMPQ)->lpFileName;
    else return FALSE;
    DWORD slen = strlen(lpFileName)+1;
    if (dwBufferLength>=slen) memcpy(lpBuffer,lpFileName,slen);
    else memcpy(lpBuffer,lpFileName,dwBufferLength);
    return TRUE;
}

BOOL SFMPQAPI WINAPI SFileOpenFile(LPCSTR lpFileName, MPQHANDLE *hFile)
{
    return SFileOpenFileEx(0,lpFileName,SFILE_SEARCH_CURRENT_ONLY,hFile);
}

BOOL WINAPI IsHexDigit(char nChar)
{
    switch (nChar) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'A':
        case 'a':
        case 'B':
        case 'b':
        case 'C':
        case 'c':
        case 'D':
        case 'd':
        case 'E':
        case 'e':
        case 'F':
        case 'f':
            return TRUE;
        default:
            return FALSE;
    }
    return FALSE;
}

BOOL SFMPQAPI WINAPI SFileOpenFileEx(MPQHANDLE hMPQ, LPCSTR lpFileName, DWORD dwSearchScope, MPQHANDLE *hFile)
{
    if (!lpFileName || !hFile) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!*lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*char szFileName[260];
    if (hMPQ!=0) {
        if ((DWORD)lpFileName<((MPQARCHIVE *)hMPQ)->MpqHeader.dwHashTableSize) {
            sprintf(szFileName,UNKNOWN_OUT,(DWORD)lpFileName);
            lpFileName = szFileName;
            dwSearchScope = SFILE_SEARCH_CURRENT_ONLY;
        }
    }
    else {
        for (DWORD i=0;i<dwOpenMpqCount;i++) {
            if ((DWORD)lpFileName<lpOpenMpq[i]->MpqHeader.dwHashTableSize) {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
        }
    }*/

    MPQFILE **lpnOpenFile = (MPQFILE **)SFAlloc(sizeof(MPQFILE *) * (dwOpenFileCount+1));
    if (!lpnOpenFile) return FALSE;

    MPQFILE *mpqOpenFile = (MPQFILE *)SFAlloc(sizeof(MPQFILE));
    if (!mpqOpenFile) {
        SFFree(lpnOpenFile);
        return FALSE;
    }
    BOOL bFileOnDisk=FALSE;
    MPQHANDLE hnMPQ,hnFile;
    TempAlloc NewAlloc;
    if (dwSearchScope==SFILE_SEARCH_CURRENT_ONLY) {
        if (hMPQ) {
            if (_memicmp(lpFileName,UNKNOWN_CMP,UNKNOWN_LEN)==0 && IsHexDigit(lpFileName[UNKNOWN_LEN])) {
                DWORD dwHashIndex;
                sscanf(lpFileName,UNKNOWN_IN,&dwHashIndex);
                hnFile = (HANDLE)&((MPQARCHIVE *)hMPQ)->lpHashTable[dwHashIndex];
                if ((((HASHTABLEENTRY *)hnFile)->dwBlockTableIndex&0xFFFFFFFE)==0xFFFFFFFE)
                {
                    SFFree(mpqOpenFile);
                    SFFree(lpnOpenFile);
                    SetLastError(ERROR_FILE_NOT_FOUND);
                    return FALSE;
                }
            }
            else hnFile = GetHashTableEntry(hMPQ,lpFileName,LocaleID);
            hnMPQ = hMPQ;
        }
        else {
            SearchOpenArchives(lpFileName,&hnMPQ,&hnFile);
        }
    }
    else {
        SearchOpenArchives(lpFileName,&hnMPQ,&hnFile);
        if (!hnFile) {
            hnMPQ = 0;
            DWORD namelen = GetFullPath(lpFileName,0,0);
            char *namebuffer = (char *)NewAlloc.Alloc(namelen);
            GetFullPath(lpFileName,namebuffer,namelen);
            lpFileName = namebuffer;
            hnFile = CreateFile(lpFileName,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
            if (hnFile==INVALID_HANDLE_VALUE) hnFile=0;
            else bFileOnDisk = TRUE;
        }
    }
    if (hnMPQ) {
        if(!((MPQARCHIVE *)hnMPQ)->lpBlockTable) {
            SFFree(mpqOpenFile);
            SFFree(lpnOpenFile);
            SetLastError(ERROR_FILE_NOT_FOUND);
            return FALSE;
        }
    }
    if (!hnFile) {
        SFFree(mpqOpenFile);
        SFFree(lpnOpenFile);
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    mpqOpenFile->lpFileName = (char *)SFAlloc(strlen(lpFileName)+1);
    if (!mpqOpenFile->lpFileName) {
        SFFree(mpqOpenFile);
        SFFree(lpnOpenFile);
        return FALSE;
    }
    if (hnMPQ) {
        if (!(((MPQARCHIVE *)hnMPQ)->lpBlockTable[((HASHTABLEENTRY *)hnFile)->dwBlockTableIndex].dwFlags & MAFA_EXISTS)) {
            SFFree(mpqOpenFile->lpFileName);
            SFFree(mpqOpenFile);
            SFFree(lpnOpenFile);
            SetLastError(ERROR_FILE_INVALID);
            return FALSE;
        }
    }
    strcpy(mpqOpenFile->lpFileName,lpFileName);
    if (!bFileOnDisk) {
        mpqOpenFile->lpParentArc = (MPQARCHIVE *)hnMPQ;
        mpqOpenFile->hFile = INVALID_HANDLE_VALUE;
        mpqOpenFile->lpHashEntry = (HASHTABLEENTRY *)hnFile;
        mpqOpenFile->lpBlockEntry = &((MPQARCHIVE *)hnMPQ)->lpBlockTable[mpqOpenFile->lpHashEntry->dwBlockTableIndex];
        mpqOpenFile->lpParentArc->dwRefCount++;
        if (mpqOpenFile->lpBlockEntry->dwFlags&MAFA_ENCRYPT) {
            LPSTR lpOldNameBuffer = mpqOpenFile->lpFileName;
            if (_memicmp(mpqOpenFile->lpFileName,UNKNOWN_CMP,UNKNOWN_LEN)==0 && IsHexDigit(mpqOpenFile->lpFileName[UNKNOWN_LEN]))
                mpqOpenFile->lpFileName = 0;
            mpqOpenFile->dwCryptKey = DetectFileSeedEx(mpqOpenFile->lpParentArc,mpqOpenFile->lpHashEntry, &mpqOpenFile->lpFileName);
            if (_memicmp(lpFileName,UNKNOWN_CMP,UNKNOWN_LEN)==0 && IsHexDigit(lpFileName[UNKNOWN_LEN])) {
                if (!mpqOpenFile->lpFileName)
                    mpqOpenFile->lpFileName = lpOldNameBuffer;
                else
                    SFFree(lpOldNameBuffer);
            }
            if (mpqOpenFile->dwCryptKey==0) {
                SFFree(mpqOpenFile->lpFileName);
                SFFree(mpqOpenFile);
                SFFree(lpnOpenFile);
                SetLastError(ERROR_FILE_INVALID);
                return FALSE;
            }
        }
    }
    else {
        mpqOpenFile->hFile = hnFile;
    }
    strncpy(mpqOpenFile->szFileName,mpqOpenFile->lpFileName,259);
    *hFile = (MPQHANDLE)mpqOpenFile;
    memcpy(lpnOpenFile,lpOpenFile,sizeof(MPQFILE *) * dwOpenFileCount);
    lpnOpenFile[dwOpenFileCount] = mpqOpenFile;
    if (lpOpenFile) SFFree(lpOpenFile);
    lpOpenFile = lpnOpenFile;
    dwOpenFileCount++;
    return TRUE;
}

BOOL SFMPQAPI WINAPI SFileCloseFile(MPQHANDLE hFile)
{
    if (!hFile) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MPQFILE *mpqOpenFile = (MPQFILE *)hFile;

    if (dwOpenFileCount==0) return FALSE;
    SFFree(mpqOpenFile->lpFileName);
    if (mpqOpenFile->lpdwBlockOffsets) SFFree(mpqOpenFile->lpdwBlockOffsets);
    if (mpqOpenFile->hFile == INVALID_HANDLE_VALUE) mpqOpenFile->lpParentArc->dwRefCount--;

    SFFree(mpqOpenFile);
    if (lpOpenFile==0) return TRUE;
    if (dwOpenFileCount-1==0) {
        SFFree(lpOpenFile);
        lpOpenFile = (MPQFILE **)0;
        dwOpenFileCount--;
        return TRUE;
    }
    else if (dwOpenFileCount==0) return FALSE;
    MPQFILE **lpnOpenFile = (MPQFILE **)SFAlloc(sizeof(MPQFILE *) * (dwOpenFileCount-1));
    if (lpnOpenFile) {
        for (DWORD i=0,j=0;i<dwOpenFileCount;i++) {
            if ((MPQHANDLE)lpOpenFile[i]!=hFile) {
                memcpy(&lpnOpenFile[j],&lpOpenFile[i],sizeof(MPQFILE *));
                j++;
            }
        }
        SFFree(lpOpenFile);
        lpOpenFile = lpnOpenFile;
        dwOpenFileCount--;
    }
    else {
        for (DWORD i=0,j=0;i<dwOpenFileCount;i++) {
            if ((MPQHANDLE)lpOpenFile[i]==hFile) {
                lpOpenFile[i] = (MPQFILE *)0;
            }
            else {
                if (i!=j) memcpy(&lpOpenFile[j],&lpOpenFile[i],sizeof(MPQFILE *));
                j++;
            }
        }
        dwOpenFileCount--;
    }
    return TRUE;
}

DWORD SFMPQAPI WINAPI SFileGetFileSize(MPQHANDLE hFile, LPDWORD lpFileSizeHigh)
{
    if (!hFile) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return (DWORD)-1;
    }

    MPQFILE *mpqOpenFile = (MPQFILE *)hFile;
    if (mpqOpenFile->hFile == INVALID_HANDLE_VALUE) {
        if (lpFileSizeHigh) *lpFileSizeHigh = 0;
        return mpqOpenFile->lpBlockEntry->dwFullSize;
    }
    else return GetFileSize(mpqOpenFile->hFile,lpFileSizeHigh);
}

BOOL SFMPQAPI WINAPI SFileGetFileArchive(MPQHANDLE hFile, MPQHANDLE *hMPQ)
{
    if (!hFile || !hMPQ) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (((MPQFILE *)hFile)->hFile == INVALID_HANDLE_VALUE) {
        *hMPQ = (MPQHANDLE)((MPQFILE *)hFile)->lpParentArc;
        return TRUE;
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
}

BOOL SFMPQAPI WINAPI SFileGetFileName(MPQHANDLE hFile, LPSTR lpBuffer, DWORD dwBufferLength)
{
    return SFileGetArchiveName(hFile,lpBuffer,dwBufferLength);
}

DWORD SFMPQAPI WINAPI SFileGetFileInfo(MPQHANDLE hFile, DWORD dwInfoType)
{
    if (!hFile) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return (DWORD)-1;
    }
    if (dwInfoType==0) {
        SetLastError(ERROR_UNKNOWN_PROPERTY);
        return (DWORD)-1;
    }

    DWORD dwhType = GetHandleType(hFile);
    if (dwhType==SFILE_TYPE_FILE)
    {
        MPQFILE *mpqOpenFile = (MPQFILE *)hFile;
        if (mpqOpenFile->hFile == INVALID_HANDLE_VALUE) {
            MPQARCHIVE *mpqOpenArc = mpqOpenFile->lpParentArc;
            switch (dwInfoType) {
            case SFILE_INFO_BLOCK_SIZE:
                return mpqOpenArc->MpqHeader.wBlockSize;
            case SFILE_INFO_HASH_TABLE_SIZE:
                return mpqOpenArc->MpqHeader.dwHashTableSize;
            case SFILE_INFO_NUM_FILES:
                return mpqOpenArc->MpqHeader.dwBlockTableSize;
            case SFILE_INFO_TYPE:
                return SFILE_TYPE_FILE;
            case SFILE_INFO_SIZE:
                return mpqOpenFile->lpBlockEntry->dwFullSize;
            case SFILE_INFO_COMPRESSED_SIZE:
                return mpqOpenFile->lpBlockEntry->dwCompressedSize;
            case SFILE_INFO_FLAGS:
                return mpqOpenFile->lpBlockEntry->dwFlags;
            case SFILE_INFO_PARENT:
                return (DWORD)mpqOpenFile->lpParentArc;
            case SFILE_INFO_POSITION:
                return mpqOpenFile->dwFilePointer;
            case SFILE_INFO_LOCALEID:
                return mpqOpenFile->lpHashEntry->lcLocale;
            case SFILE_INFO_PRIORITY:
                return mpqOpenArc->dwPriority;
            case SFILE_INFO_HASH_INDEX:
                return ((UIntPtr)mpqOpenFile->lpHashEntry-(UIntPtr)mpqOpenArc->lpHashTable)/sizeof(HASHTABLEENTRY);
            case SFILE_INFO_BLOCK_INDEX:
                return mpqOpenFile->lpHashEntry->dwBlockTableIndex;
            default:
                SetLastError(ERROR_UNKNOWN_PROPERTY);
                return (DWORD)-1;
            }
        }
        else {
            switch (dwInfoType) {
            case SFILE_INFO_TYPE:
                return SFILE_TYPE_FILE;
            case SFILE_INFO_SIZE:
                return (DWORD)SFGetFileSize(mpqOpenFile->hFile);
            case SFILE_INFO_COMPRESSED_SIZE:
                return (DWORD)SFGetFileSize(mpqOpenFile->hFile);
#ifdef _WIN32
            case SFILE_INFO_FLAGS:
                return GetFileAttributes(mpqOpenFile->lpFileName);
#endif
            case SFILE_INFO_POSITION:
                return (DWORD)SFSetFilePointer(mpqOpenFile->hFile,0,FILE_CURRENT);
            default:
                SetLastError(ERROR_UNKNOWN_PROPERTY);
                return (DWORD)-1;

            }
        }
    }
    else if (dwhType==SFILE_TYPE_MPQ)
    {
        MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hFile;
        switch (dwInfoType) {
        case SFILE_INFO_BLOCK_SIZE:
            return mpqOpenArc->MpqHeader.wBlockSize;
        case SFILE_INFO_HASH_TABLE_SIZE:
            return mpqOpenArc->MpqHeader.dwHashTableSize;
        case SFILE_INFO_NUM_FILES:
            return mpqOpenArc->MpqHeader.dwBlockTableSize;
        case SFILE_INFO_TYPE:
            return SFILE_TYPE_MPQ;
        case SFILE_INFO_SIZE:
            return mpqOpenArc->MpqHeader.dwMPQSize;
        case SFILE_INFO_PRIORITY:
            return mpqOpenArc->dwPriority;
        default:
            SetLastError(ERROR_UNKNOWN_PROPERTY);
            return (DWORD)-1;
        }
    }
    SetLastError(ERROR_INVALID_PARAMETER);
    return (DWORD)-1;
}

DWORD SFMPQAPI WINAPI SFileSetFilePointer(MPQHANDLE hFile, LONG lDistanceToMove, PLONG lplDistanceToMoveHigh, DWORD dwMoveMethod)
{
    if (!hFile) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return (DWORD)-1;
    }

    MPQFILE *mpqOpenFile = (MPQFILE *)hFile;
    if (mpqOpenFile->hFile == INVALID_HANDLE_VALUE) {
        long fsz = mpqOpenFile->lpBlockEntry->dwFullSize;
        long cpos = mpqOpenFile->dwFilePointer;
        switch (dwMoveMethod) {
        case FILE_CURRENT:
            if (cpos + lDistanceToMove < 0 || cpos + lDistanceToMove > fsz) return (DWORD)-1;
            mpqOpenFile->dwFilePointer += lDistanceToMove;
            break;

        case FILE_END:
            if (fsz + lDistanceToMove < 0 || lDistanceToMove > 0) return (DWORD)-1;
            mpqOpenFile->dwFilePointer = fsz + lDistanceToMove;
            break;
        case FILE_BEGIN:
        default:
            if (lDistanceToMove < 0 || lDistanceToMove > fsz) return (DWORD)-1;
            mpqOpenFile->dwFilePointer = lDistanceToMove;
        }
        if (lplDistanceToMoveHigh!=0) *lplDistanceToMoveHigh = 0;
        return mpqOpenFile->dwFilePointer;
    }
    else return SetFilePointer(mpqOpenFile->hFile,lDistanceToMove,lplDistanceToMoveHigh,dwMoveMethod);

}

BOOL SFMPQAPI WINAPI SFileReadFile(MPQHANDLE hFile,LPVOID lpBuffer,DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead,LPOVERLAPPED lpOverlapped)
{
    if (!hFile || !lpBuffer) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MPQFILE *mpqOpenFile = (MPQFILE *)hFile;
    if (mpqOpenFile->hFile != INVALID_HANDLE_VALUE) {
        DWORD tsz=0;
        BOOL RetVal = ReadFile(mpqOpenFile->hFile,lpBuffer,nNumberOfBytesToRead,&tsz,lpOverlapped);
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = tsz;
        return RetVal;
    }
    if (lpOverlapped)
        if (lpOverlapped->Internal || lpOverlapped->InternalHigh || lpOverlapped->Offset || lpOverlapped->OffsetHigh || lpOverlapped->hEvent)
            SFileSetFilePointer(hFile,lpOverlapped->Offset,(LONG *)&lpOverlapped->OffsetHigh,FILE_BEGIN);
    if (nNumberOfBytesToRead==0) {
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        if (lpOverlapped) lpOverlapped->InternalHigh = 0;
        return TRUE;
    }
    MPQARCHIVE *mpqOpenArc = mpqOpenFile->lpParentArc;

    if (mpqOpenFile->dwFilePointer>mpqOpenFile->lpBlockEntry->dwFullSize) return FALSE;
    if (mpqOpenFile->dwFilePointer==mpqOpenFile->lpBlockEntry->dwFullSize)
    {
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        if (lpOverlapped) lpOverlapped->InternalHigh = 0;
        return TRUE;
    }
    DWORD nBytesRead,TotalBytesRead=0;
    DWORD BlockIndex = mpqOpenFile->lpHashEntry->dwBlockTableIndex;
    if (mpqOpenFile->dwFilePointer+nNumberOfBytesToRead>mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize) nNumberOfBytesToRead = mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize-mpqOpenFile->dwFilePointer;
    DWORD blockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
    if (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_SINGLEBLOCK)
        blockSize = mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize;
    DWORD dwBlockStart = mpqOpenFile->dwFilePointer - (mpqOpenFile->dwFilePointer % blockSize);
    DWORD blockNum = dwBlockStart / blockSize;
    DWORD nBlocks  = (mpqOpenFile->dwFilePointer+nNumberOfBytesToRead) / blockSize;
    if((mpqOpenFile->dwFilePointer+nNumberOfBytesToRead) % blockSize)
        nBlocks++;
    DWORD TotalBlocks = mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize / blockSize;
    if(mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize % blockSize)
        TotalBlocks++;
    DWORD dwBlockPos = mpqOpenFile->dwFilePointer % blockSize;
    DWORD dwBlockPosEnd = (mpqOpenFile->dwFilePointer+nNumberOfBytesToRead) % blockSize;
    if (dwBlockPosEnd==0) dwBlockPosEnd = blockSize;
    void *blkBuffer = SFAlloc(blockSize);
    if (!blkBuffer) {
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        if (lpOverlapped) lpOverlapped->InternalHigh = 0;
        return FALSE;
    }
    DWORD nBufferCount;
    for (nBufferCount = 1; nBufferCount < nBlocks - blockNum && blockSize * nBufferCount < 65536; nBufferCount *= 2) {}
    if (nBufferCount > nBlocks - blockNum) nBufferCount = nBlocks - blockNum;
    void *blkLargeBuffer = NULL;
    if (nBufferCount > 1) {
        blkLargeBuffer = SFAlloc(blockSize * nBufferCount);
        if (!blkLargeBuffer) {
            SFFree(blkBuffer);
            if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
            if (lpOverlapped) lpOverlapped->InternalHigh = 0;
            return FALSE;
        }
    }
    DWORD dwCryptKey = mpqOpenFile->dwCryptKey;
    //if (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_ENCRYPT) dwCryptKey = HashString(mpqOpenFile->lpFileName,HASH_KEY);
    //if (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_MODCRYPTKEY) dwCryptKey = (dwCryptKey + mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset) ^ mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize;
    DWORD HeaderLength=0;
    if (memcmp(&mpqOpenArc->MpqHeader.dwMPQID,ID_BN3,4)==0)
    {
        SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset,FILE_BEGIN);
        ReadFile(mpqOpenArc->hFile,&HeaderLength,4,&nBytesRead,0);
    }
    DWORD i;
    if (!mpqOpenFile->lpdwBlockOffsets)
    {
        DWORD *dwBlockPtrTable = (DWORD *)SFAlloc((TotalBlocks+1)*4);
        if (!dwBlockPtrTable) {

            if (blkLargeBuffer) SFFree(blkLargeBuffer);
            SFFree(blkBuffer);
            if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
            if (lpOverlapped) lpOverlapped->InternalHigh = 0;
            return FALSE;
        }
        if (((mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_COMPRESS) || (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_COMPRESS2)) && !(mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_SINGLEBLOCK))
        {
            SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+HeaderLength,FILE_BEGIN);
            ReadFile(mpqOpenArc->hFile,dwBlockPtrTable,(TotalBlocks+1)*4,&nBytesRead,0);
            if (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_ENCRYPT) {
                DecryptData((LPBYTE)dwBlockPtrTable,(TotalBlocks+1)*4,dwCryptKey-1);
            }
        }
        else
        {
            for (i=0;i<TotalBlocks+1;i++) {
                if (i<TotalBlocks) dwBlockPtrTable[i] = i * blockSize;
                else dwBlockPtrTable[i] = mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize - HeaderLength;
            }
        }
        mpqOpenFile->lpdwBlockOffsets = dwBlockPtrTable;
    }
    BYTE *compbuffer = NULL;
    if ((mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_COMPRESS) || (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_COMPRESS2)) {
        compbuffer = (BYTE *)SFAlloc(blockSize+3);
        if (!compbuffer) {
            if (blkLargeBuffer) SFFree(blkLargeBuffer);
            SFFree(blkBuffer);
            if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
            if (lpOverlapped) lpOverlapped->InternalHigh = 0;
            return FALSE;
        }
    }
    DWORD blk=0,blki=0;
    for (i=blockNum;i<nBlocks;i++) {
        if (blk==0 && blkLargeBuffer) {
            SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+HeaderLength+mpqOpenFile->lpdwBlockOffsets[i],FILE_BEGIN);
            blki=i;
            if (i+nBufferCount>nBlocks) {
                if (ReadFile(mpqOpenArc->hFile,blkLargeBuffer,mpqOpenFile->lpdwBlockOffsets[nBlocks]-mpqOpenFile->lpdwBlockOffsets[i],&nBytesRead,0)==0) {
                    if (compbuffer) SFFree(compbuffer);
                    SFFree(blkLargeBuffer);
                    SFFree(blkBuffer);
                    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = TotalBytesRead;
                    if (lpOverlapped) lpOverlapped->InternalHigh = TotalBytesRead;
                    return FALSE;
                }

            }
            else {
                if (ReadFile(mpqOpenArc->hFile,blkLargeBuffer,mpqOpenFile->lpdwBlockOffsets[i+nBufferCount]-mpqOpenFile->lpdwBlockOffsets[i],&nBytesRead,0)==0) {
                    if (compbuffer) SFFree(compbuffer);
                    SFFree(blkLargeBuffer);
                    SFFree(blkBuffer);
                    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = TotalBytesRead;
                    if (lpOverlapped) lpOverlapped->InternalHigh = TotalBytesRead;
                    return FALSE;
                }
            }
        }
        if (blkLargeBuffer) {
            memcpy(blkBuffer,((char *)blkLargeBuffer)+(mpqOpenFile->lpdwBlockOffsets[blki+blk]-mpqOpenFile->lpdwBlockOffsets[blki]),mpqOpenFile->lpdwBlockOffsets[i+1]-mpqOpenFile->lpdwBlockOffsets[i]);
        }
        else {
            SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+HeaderLength+mpqOpenFile->lpdwBlockOffsets[i],FILE_BEGIN);
            blki=i;
            if (ReadFile(mpqOpenArc->hFile,blkBuffer,mpqOpenFile->lpdwBlockOffsets[i+1]-mpqOpenFile->lpdwBlockOffsets[i],&nBytesRead,0)==0) {
                if (compbuffer) SFFree(compbuffer);
                SFFree(blkBuffer);
                if (lpNumberOfBytesRead) *lpNumberOfBytesRead = TotalBytesRead;
                if (lpOverlapped) lpOverlapped->InternalHigh = TotalBytesRead;
                return FALSE;
            }
        }
        if (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_ENCRYPT)
        {
            DecryptData((LPBYTE)blkBuffer,mpqOpenFile->lpdwBlockOffsets[i+1]-mpqOpenFile->lpdwBlockOffsets[i],dwCryptKey+i);
        }
        if (i==TotalBlocks-1 && (mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize % blockSize)!=0) blockSize=mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize % blockSize;
        DWORD outLength=blockSize;
        if ((mpqOpenFile->lpdwBlockOffsets[i+1]-mpqOpenFile->lpdwBlockOffsets[i])!=blockSize)
        {
            if (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_COMPRESS)
            {
                BYTE *compdata = compbuffer;
                memcpy(compdata,(char *)blkBuffer,mpqOpenFile->lpdwBlockOffsets[i+1]-mpqOpenFile->lpdwBlockOffsets[i]);
                if (compdata[0]&UNSUPPORTED_DECOMPRESSION) {
                    LoadStorm();
                    if (stormSCompDecompress)
                        stormSCompDecompress(blkBuffer, &outLength, compdata, mpqOpenFile->lpdwBlockOffsets[i+1]-mpqOpenFile->lpdwBlockOffsets[i]);
                }
                else {
                    SCompDecompress(blkBuffer, &outLength, compdata, mpqOpenFile->lpdwBlockOffsets[i+1]-mpqOpenFile->lpdwBlockOffsets[i]);
                }
            }
            else if (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_COMPRESS2)
            {
                BYTE *compdata = compbuffer;
                compdata[0] = 0x08;
                memcpy(compdata+1,(char *)blkBuffer,mpqOpenFile->lpdwBlockOffsets[i+1]-mpqOpenFile->lpdwBlockOffsets[i]);
                SCompDecompress(blkBuffer, &outLength, compdata, mpqOpenFile->lpdwBlockOffsets[i+1]-mpqOpenFile->lpdwBlockOffsets[i]+1);
            }
        }
        if (i==blockNum) {
            if (nNumberOfBytesToRead > blockSize-dwBlockPos) {
                memcpy(lpBuffer,(char *)blkBuffer+dwBlockPos,blockSize-dwBlockPos);
                TotalBytesRead+=blockSize-dwBlockPos;
                mpqOpenFile->dwFilePointer += blockSize-dwBlockPos;
                nNumberOfBytesToRead-=blockSize-dwBlockPos;
            }
            else {
                memcpy(lpBuffer,(char *)blkBuffer+dwBlockPos,nNumberOfBytesToRead);
                TotalBytesRead+=nNumberOfBytesToRead;
                mpqOpenFile->dwFilePointer += nNumberOfBytesToRead;
                nNumberOfBytesToRead-=nNumberOfBytesToRead;
            }
        }
        else if (i==nBlocks-1) {
            if (nNumberOfBytesToRead > dwBlockPosEnd) {
                memcpy((char *)lpBuffer+TotalBytesRead,blkBuffer,dwBlockPosEnd);
                TotalBytesRead+=dwBlockPosEnd;
                mpqOpenFile->dwFilePointer += dwBlockPosEnd;
                nNumberOfBytesToRead-=dwBlockPosEnd;
            }
            else {
                memcpy((char *)lpBuffer+TotalBytesRead,blkBuffer,nNumberOfBytesToRead);
                TotalBytesRead+=nNumberOfBytesToRead;
                mpqOpenFile->dwFilePointer += nNumberOfBytesToRead;
                nNumberOfBytesToRead-=nNumberOfBytesToRead;
            }
        }
        else {
            if (nNumberOfBytesToRead > blockSize) {
                memcpy((char *)lpBuffer+TotalBytesRead,blkBuffer,blockSize);
                TotalBytesRead+=blockSize;
                mpqOpenFile->dwFilePointer += blockSize;
                nNumberOfBytesToRead-=blockSize;
            }
            else {
                memcpy((char *)lpBuffer+TotalBytesRead,blkBuffer,nNumberOfBytesToRead);
                TotalBytesRead+=nNumberOfBytesToRead;
                mpqOpenFile->dwFilePointer += nNumberOfBytesToRead;
                nNumberOfBytesToRead-=nNumberOfBytesToRead;
            }
        }
        blk = (blk+1) % nBufferCount;
    }
    if (compbuffer) SFFree(compbuffer);
    if (blkLargeBuffer) SFFree(blkLargeBuffer);
    SFFree(blkBuffer);
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = TotalBytesRead;
    if (lpOverlapped) lpOverlapped->InternalHigh = TotalBytesRead;
    return TRUE;
}

LCID SFMPQAPI WINAPI SFileSetLocale(LCID nNewLocale)
{
    return (LocaleID = nNewLocale);
}

BOOL SFMPQAPI WINAPI SFileGetBasePath(LPSTR lpBuffer, DWORD dwBufferLength)
{
    if (lpBuffer==0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    DWORD slen = strlen(StormBasePath)+1;
    if (dwBufferLength>=slen) memcpy((void *)lpBuffer,StormBasePath,slen);
    else memcpy((void *)lpBuffer,StormBasePath,dwBufferLength);
    return TRUE;
}

BOOL SFMPQAPI WINAPI SFileSetBasePath(LPCSTR lpNewBasePath)
{
    if (!lpNewBasePath) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!*lpNewBasePath) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    DWORD slen = strlen(lpNewBasePath)+1;
    if (slen>MAX_PATH+1) return FALSE;
    else if (slen==MAX_PATH+1)
        if (lpNewBasePath[MAX_PATH-1]!='\\' && lpNewBasePath[MAX_PATH-1]!='/') return FALSE;
    TempAlloc NewAlloc;
    char *buffer = (char *)NewAlloc.Alloc(slen+1);
    memcpy(buffer,lpNewBasePath,slen);
    char *npath;
#ifdef _WIN32
    for (npath=buffer;npath[0]!=0;npath++)
        if (npath[0]=='/') npath[0]='\\';
    if (npath[-1]!='\\') {
        npath[0]='\\';
        npath[1]=0;
        slen+=1;
    }
#else
    for (npath=buffer;npath[0]!=0;npath++)
        if (npath[0]=='\\') npath[0]='/';
    if (npath[-1]!='/') {
        npath[0]='/';
        npath[1]=0;
        slen+=1;
    }
#endif
#ifdef _WIN32
    DWORD attrib = GetFileAttributes(buffer);
    if ((attrib&FILE_ATTRIBUTE_DIRECTORY)==0 || attrib==0xFFFFFFFF) return FALSE;
#endif

    memcpy(StormBasePath,buffer,slen);
    return TRUE;
}

BOOL SFMPQAPI WINAPI SFileSetArchivePriority(MPQHANDLE hMPQ, DWORD dwPriority)
{
    if (!hMPQ) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    mpqOpenArc->dwPriority = dwPriority;
    if (dwOpenMpqCount>1) SortOpenArchivesByPriority();
    return TRUE;
}

int StringICompare(const void *arg1,const void *arg2)
{
    return _stricmp(*(const char * const *)arg1,*(const char * const *)arg2);
}

BOOL SFMPQAPI WINAPI SFileListFiles(MPQHANDLE hMPQ, LPCSTR lpFileLists, FILELISTENTRY *lpListBuffer, DWORD dwFlags)
{
    if (!hMPQ || !lpListBuffer) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    DWORD i,tsz;

    if (memcmp(&mpqOpenArc->MpqHeader.dwMPQID,ID_BN3,4)==0)
    {
        for (i=0;i<mpqOpenArc->MpqHeader.dwHashTableSize;i++) {
            lpListBuffer[i].dwFileExists = 0;
            lpListBuffer[i].szFileName[0] = 0;
            if ((mpqOpenArc->lpHashTable[i].dwBlockTableIndex&0xFFFFFFFE)!=0xFFFFFFFE && mpqOpenArc->lpHashTable[i].dwBlockTableIndex < mpqOpenArc->MpqHeader.dwBlockTableSize) {
                lpListBuffer[i].lcLocale = mpqOpenArc->lpHashTable[i].lcLocale;
                DWORD dwBlockIndex = mpqOpenArc->lpHashTable[i].dwBlockTableIndex;
                lpListBuffer[i].dwCompressedSize = mpqOpenArc->lpBlockTable[dwBlockIndex].dwCompressedSize;
                lpListBuffer[i].dwFullSize = mpqOpenArc->lpBlockTable[dwBlockIndex].dwFullSize;
                lpListBuffer[i].dwFlags = mpqOpenArc->lpBlockTable[dwBlockIndex].dwFlags;
                if (dwFlags & SFILE_LIST_FLAG_UNKNOWN)
                    lpListBuffer[i].dwFileExists = 1;
                else
                    lpListBuffer[i].dwFileExists=0xFFFFFFFF;
                SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[dwBlockIndex].dwFileOffset+0x40,FILE_BEGIN);
                ReadFile(mpqOpenArc->hFile,lpListBuffer[i].szFileName,260,&tsz,0);

                if (mpqOpenArc->lpHashTable[i].dwNameHashA==HashString(lpListBuffer[i].szFileName,HASH_NAME_A) && mpqOpenArc->lpHashTable[i].dwNameHashB==HashString(lpListBuffer[i].szFileName,HASH_NAME_B)) {
                    if (dwFlags&SFILE_LIST_ONLY_UNKNOWN && !(dwFlags&SFILE_LIST_ONLY_KNOWN)) {
                        lpListBuffer[i].dwFileExists = 0;
                    }
                }
                else {
                    sprintf(lpListBuffer[i].szFileName,UNKNOWN_OUT,i);
                    if (dwFlags & SFILE_LIST_FLAG_UNKNOWN) {
                        lpListBuffer[i].dwFileExists |= 2;
                    }

                    if (dwFlags&SFILE_LIST_ONLY_KNOWN) {
                        lpListBuffer[i].dwFileExists = 0;
                    }
                }
            }
        }

        return TRUE;
    }

    char **lpNameBuffers=0;

    char **lpNames=0,szNull[1]={0},*lpFLCopy;
    DWORD *lpdwNameHashA=0,*lpdwNameHashB=0;
    DWORD dwListNameHashA,dwListNameHashB;
    DWORD j;
    DWORD dwExtLists=0,dwIntLists=0;
    if (!lpFileLists) lpFileLists=szNull;
    lpFLCopy = _strdup(lpFileLists);
    for (char* lpLines=lpFLCopy;lpLines;lpLines=nextline(lpLines)) {
        if (!*lpLines) break;
        dwExtLists++;
    }
    dwListNameHashA = HashString(INTERNAL_LISTFILE,HASH_NAME_A);
    dwListNameHashB = HashString(INTERNAL_LISTFILE,HASH_NAME_B);
    for (i=0;i<mpqOpenArc->MpqHeader.dwHashTableSize;i++) {
        if (mpqOpenArc->lpHashTable[i].dwNameHashA==dwListNameHashA && mpqOpenArc->lpHashTable[i].dwNameHashB==dwListNameHashB && (mpqOpenArc->lpHashTable[i].dwBlockTableIndex&0xFFFFFFFE)!=0xFFFFFFFE) {
            dwIntLists++;
        }
    }
    lpNameBuffers = (char**)SFAlloc((1+dwExtLists+dwIntLists)*sizeof(char *));
    if (dwExtLists) {
        i=0;
        for (char* lpLines=lpFLCopy;lpLines;lpLines=nextline(lpLines)) {
            if (!*lpLines) break;
            lpNameBuffers[i+1] = lpLines;
            i++;
        }
        for (i=0;i<dwExtLists;i++) {
            lpNameBuffers[i+1][strlnlen(lpNameBuffers[i+1])]=0;
        }
        qsort(lpNameBuffers+1,dwExtLists,sizeof(char *),StringICompare);
        for (i=0;i<dwExtLists-1;i++) {
            if (_stricmp(lpNameBuffers[i+1],lpNameBuffers[i+2])==0) {
                memmove(&lpNameBuffers[i+1],&lpNameBuffers[i+2],(dwExtLists-(i+1))*sizeof(char *));
                dwExtLists--;
            }
        }
    }
    if (dwIntLists) {
        dwIntLists = 0;
        for (i=0;i<mpqOpenArc->MpqHeader.dwHashTableSize;i++) {
            if (mpqOpenArc->lpHashTable[i].dwNameHashA==dwListNameHashA && mpqOpenArc->lpHashTable[i].dwNameHashB==dwListNameHashB && (mpqOpenArc->lpHashTable[i].dwBlockTableIndex&0xFFFFFFFE)!=0xFFFFFFFE) {
                lpNameBuffers[1+dwExtLists+dwIntLists] = (char *)i;
                dwIntLists++;
            }
        }
    }
    DWORD dwLines=0,dwOldLines=0,dwTotalLines=0;
    DWORD fsz;
    HANDLE hFile;
    DWORD dwListCryptKey = HashString(INTERNAL_LISTFILE,HASH_KEY);
    for (i=0;i<1+dwExtLists+dwIntLists;i++) {
        if (i==0) {
            char* files;
            fsz = strlen(INTERNAL_FILES);
            files = (char*)SFAlloc(fsz+1);
            memcpy(files,INTERNAL_FILES,fsz);
            files[fsz]='\0';
            lpNameBuffers[i] = files;
        }
        else if (i<1+dwExtLists) {
            if (!(dwFlags&SFILE_LIST_MEMORY_LIST)) {
                hFile = CreateFile(lpNameBuffers[i],GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
                if (hFile!=INVALID_HANDLE_VALUE) {
                    char* contents;
                    fsz = (DWORD)SFGetFileSize(hFile);
                    SFSetFilePointer(hFile,0,FILE_BEGIN);
                    contents = (char *)SFAlloc(fsz+1);
                    ReadFile(hFile,contents,fsz,&tsz,0);
                    CloseHandle(hFile);
                    contents[fsz]='\0';
                    lpNameBuffers[i] = contents;
                }
                else lpNameBuffers[i]=NULL;
            }
            else {
                dwTotalLines++;
                continue;
            }
        }
        else {
            MPQFILE thisFile;
            memset(&thisFile,0,sizeof(MPQFILE));
            thisFile.lpParentArc = (MPQARCHIVE *)hMPQ;
            thisFile.hFile = INVALID_HANDLE_VALUE;
            thisFile.lpHashEntry = &mpqOpenArc->lpHashTable[(DWORD)lpNameBuffers[i]];
            thisFile.lpBlockEntry = &mpqOpenArc->lpBlockTable[thisFile.lpHashEntry->dwBlockTableIndex];
            thisFile.lpFileName = thisFile.szFileName;
            strcpy(thisFile.lpFileName,INTERNAL_LISTFILE);
            thisFile.dwFilePointer = 0;
            thisFile.lpdwBlockOffsets = 0;
            BLOCKTABLEENTRY *lpBlockEntry = thisFile.lpBlockEntry;
            if (lpBlockEntry->dwFlags & MAFA_ENCRYPT) {
                thisFile.dwCryptKey = dwListCryptKey;
                if (lpBlockEntry->dwFlags & MAFA_MODCRYPTKEY)
                    thisFile.dwCryptKey = (thisFile.dwCryptKey + lpBlockEntry->dwFileOffset) ^ lpBlockEntry->dwFullSize;
            }
            fsz = lpBlockEntry->dwFullSize;
            char* contents;
            contents = (char *)SFAlloc(fsz+1);
            SFileReadFile(&thisFile,contents,fsz,0,0);
            if (thisFile.lpdwBlockOffsets) SFFree(thisFile.lpdwBlockOffsets);
            contents[fsz]='\0';
            lpNameBuffers[i] = contents;
        }
        for (char* lpLines=lpNameBuffers[i];lpLines;lpLines=nextline(lpLines)) {
            if (!*lpLines) break;
            dwTotalLines++;
        }
    }
    lpNames = (char **)SFAlloc(dwTotalLines*sizeof(char *));
    for (i=0;i<1+dwExtLists+dwIntLists;i++) {
        char *lpLines;
        dwOldLines=dwLines;
        for (lpLines=lpNameBuffers[i];lpLines;lpLines=nextline(lpLines)) {
            if (!*lpLines) break;
            lpNames[dwLines] = lpLines;
            dwLines++;
        }
        for (j=dwOldLines;j<dwLines;j++) {
            lpNames[j][strlnlen(lpNames[j])]=0;
        }
    }
    qsort(lpNames,dwTotalLines,sizeof(char *),StringICompare);
    for (i=0;i<dwTotalLines-1;i++) {
        if (_stricmp(lpNames[i],lpNames[i+1])==0) {
            memmove(&lpNames[i],&lpNames[i+1],(dwTotalLines-(i+1))*sizeof(char *));
            dwTotalLines--;
        }
    }
    if (dwTotalLines) {
        lpdwNameHashA = (DWORD *)SFAlloc(dwTotalLines*sizeof(DWORD));
        lpdwNameHashB = (DWORD *)SFAlloc(dwTotalLines*sizeof(DWORD));
        for (i=0;i<dwTotalLines;i++) {
            lpdwNameHashA[i] = HashString(lpNames[i],HASH_NAME_A);
            lpdwNameHashB[i] = HashString(lpNames[i],HASH_NAME_B);
        }
    }
    for (i=0;i<mpqOpenArc->MpqHeader.dwHashTableSize;i++) {
        lpListBuffer[i].dwFileExists = 0;
        lpListBuffer[i].szFileName[0] = 0;
        if ((mpqOpenArc->lpHashTable[i].dwBlockTableIndex&0xFFFFFFFE)!=0xFFFFFFFE && mpqOpenArc->lpHashTable[i].dwBlockTableIndex < mpqOpenArc->MpqHeader.dwBlockTableSize) {
            lpListBuffer[i].lcLocale = mpqOpenArc->lpHashTable[i].lcLocale;
            DWORD dwBlockIndex = mpqOpenArc->lpHashTable[i].dwBlockTableIndex;
            lpListBuffer[i].dwCompressedSize = mpqOpenArc->lpBlockTable[dwBlockIndex].dwCompressedSize;
            lpListBuffer[i].dwFullSize = mpqOpenArc->lpBlockTable[dwBlockIndex].dwFullSize;
            lpListBuffer[i].dwFlags = mpqOpenArc->lpBlockTable[dwBlockIndex].dwFlags;
            if (dwFlags & SFILE_LIST_FLAG_UNKNOWN)
                lpListBuffer[i].dwFileExists = 1;
            else
                lpListBuffer[i].dwFileExists=0xFFFFFFFF;
            for (j=0;j<dwTotalLines;j++) {
                if (mpqOpenArc->lpHashTable[i].dwNameHashA==lpdwNameHashA[j] && mpqOpenArc->lpHashTable[i].dwNameHashB==lpdwNameHashB[j]) {
                    strncpy(lpListBuffer[i].szFileName,lpNames[j],260);
                    if (dwFlags&SFILE_LIST_ONLY_UNKNOWN && !(dwFlags&SFILE_LIST_ONLY_KNOWN)) {
                        lpListBuffer[i].dwFileExists = 0;
                    }
                    break;
                }
                if (j+1==dwTotalLines) {
                    sprintf(lpListBuffer[i].szFileName,UNKNOWN_OUT,i);
                    if (dwFlags & SFILE_LIST_FLAG_UNKNOWN) {
                        lpListBuffer[i].dwFileExists |= 2;
                    }

                    if (dwFlags&SFILE_LIST_ONLY_KNOWN) {
                        lpListBuffer[i].dwFileExists = 0;
                    }
                }
            }
        }
    }
    if (lpNameBuffers) {
        for (i=0;i<1+dwExtLists+dwIntLists;i++) {
            if (i>=1 && i<1+dwExtLists) {
                if ((dwFlags&SFILE_LIST_MEMORY_LIST)) {
                    continue;
                }
            }
            if (lpNameBuffers[i]) SFFree(lpNameBuffers[i]);
        }
        SFFree(lpNameBuffers);
    }
    SFFree(lpFLCopy);
    if (lpdwNameHashA) SFFree(lpdwNameHashA);
    if (lpdwNameHashB) SFFree(lpdwNameHashB);
    return TRUE;
}

MPQHANDLE SFMPQAPI WINAPI MpqOpenArchiveForUpdate(LPCSTR lpFileName, DWORD dwFlags, DWORD dwMaximumFilesInArchive)
{
    MPQHANDLE hMPQ;

    MpqOpenArchiveEx(lpFileName,0,0,&hMPQ,dwFlags,dwMaximumFilesInArchive,USE_DEFAULT_BLOCK_SIZE);
    return hMPQ;
}

MPQHANDLE SFMPQAPI WINAPI MpqOpenArchiveForUpdateEx(LPCSTR lpFileName, DWORD dwFlags, DWORD dwMaximumFilesInArchive, DWORD dwBlockSize)
{
    MPQHANDLE hMPQ;

    MpqOpenArchiveEx(lpFileName,0,0,&hMPQ,dwFlags,dwMaximumFilesInArchive,dwBlockSize);
    return hMPQ;
}

DWORD SFMPQAPI WINAPI MpqCloseUpdatedArchive(MPQHANDLE hMPQ, DWORD dwUnknown2)
{
    return SFileCloseArchive(hMPQ);
}

BOOL SFMPQAPI WINAPI MpqAddFileToArchiveEx(MPQHANDLE hMPQ, LPCSTR lpSourceFileName, LPCSTR lpDestFileName, DWORD dwFlags, DWORD dwCompressionType, DWORD dwCompressLevel)
{
    if (!hMPQ || !lpSourceFileName || !lpDestFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!*lpSourceFileName || !*lpDestFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!(((MPQARCHIVE *)hMPQ)->dwFlags&SFILE_OPEN_ALLOW_WRITE)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    BOOL ReplaceExisting;
    if (dwFlags&MAFA_REPLACE_EXISTING) {
        dwFlags = dwFlags^MAFA_REPLACE_EXISTING;
        ReplaceExisting = TRUE;
    }
    else ReplaceExisting = FALSE;
    MPQHANDLE hFile = GetFreeHashTableEntry(hMPQ,lpDestFileName,LocaleID,ReplaceExisting);
    if (!hFile) return FALSE;
    HANDLE haFile = CreateFile(lpSourceFileName,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
    if (haFile==INVALID_HANDLE_VALUE) return FALSE;
    DWORD fsz = (DWORD)SFGetFileSize(haFile),tsz;
    DWORD ucfsz = fsz;
    BOOL IsBNcache = FALSE;char *buffer,*hbuffer = nullptr;
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    DWORD blockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
    DWORD TotalBlocks = fsz / blockSize;
    if(fsz % blockSize)
        TotalBlocks++;
    DWORD ptsz = 0;
    if (dwFlags&MAFA_COMPRESS || dwFlags&MAFA_COMPRESS2) ptsz = (TotalBlocks+1)*4;
    if (memcmp(&mpqOpenArc->MpqHeader.dwMPQID,ID_BN3,4)==0)
    {
        IsBNcache = TRUE;
        hbuffer = (char *)SFAlloc(fsz+324+ptsz);
        if (!hbuffer) {
            CloseHandle(haFile);
            return FALSE;
        }
        hbuffer[0] = (char)0x44;
        hbuffer[1] = (char)0x01;
        DWORD lpFileNameLength = strlen(lpDestFileName);
        if (lpFileNameLength<=260) memcpy(hbuffer+64,lpDestFileName,lpFileNameLength);
        else strncpy(hbuffer+64,lpDestFileName,260);
        buffer = hbuffer+324;
    }
    else buffer = (char *)SFAlloc(fsz+ptsz);
    if (!buffer && fsz!=0) {
        CloseHandle(haFile);
        return FALSE;
    }
    if (fsz!=0) {
        if (ReadFile(haFile,buffer,fsz,&tsz,0)==0) {
            SFFree(buffer);
            CloseHandle(haFile);
            return FALSE;
        }
    }
    else {
        if (dwFlags&MAFA_COMPRESS) dwFlags = dwFlags^MAFA_COMPRESS;
        if (dwFlags&MAFA_COMPRESS2) dwFlags = dwFlags^MAFA_COMPRESS2;
        if (dwFlags&MAFA_ENCRYPT) dwFlags = dwFlags^MAFA_ENCRYPT;
        if (dwFlags&MAFA_MODCRYPTKEY) dwFlags = dwFlags^MAFA_MODCRYPTKEY;
    }
    CloseHandle(haFile);
    DWORD i;
    DWORD *dwBlkpt = (DWORD *)SFAlloc((TotalBlocks+1)*4);
    if (!dwBlkpt) {
        SFFree(buffer);
        return FALSE;
    }
    if ((dwFlags & MAFA_COMPRESS) || (dwFlags & MAFA_COMPRESS2))
    {
        DWORD dwCompressionSubType = 0;

        if ((dwCompressionType&0x40 || dwCompressionType&0x80) && (memcmp(buffer,ID_RIFF,4)!=0 || memcmp(buffer+8,ID_WAVE,8)!=0 || buffer[20]!=1 || buffer[34]!=16)) {
            dwCompressionType = 0x08;
            dwCompressLevel = 0;
        }
        if (dwCompressionType&0x40 || dwCompressionType&0x80) {
            switch (dwCompressLevel) {
                case MAWA_QUALITY_HIGH:
                    dwCompressLevel = 1;
                    break;
                case MAWA_QUALITY_LOW:
                    dwCompressLevel = 3;
                    break;
                default:
                    dwCompressLevel = 0;
            }
        }
        else if (dwCompressionType&0x01 || dwCompressionType&0x08 || ((dwFlags & MAFA_COMPRESS)!=MAFA_COMPRESS && (dwFlags & MAFA_COMPRESS2)==MAFA_COMPRESS2)) {
            dwCompressionSubType = dwCompressLevel;
            dwCompressLevel = 0;
        }
        else if (dwCompressionType&0x02) {
            dwCompressionSubType = 1;
        }

        DWORD LastOffset=ptsz;
        BYTE *compbuffer = (BYTE *)SFAlloc(blockSize);
        if (!compbuffer) {
            SFFree(dwBlkpt);
            SFFree(buffer);
            return FALSE;
        }
        char *outbuffer = (char *)SFAlloc(blockSize);
        if (!outbuffer) {
            SFFree(compbuffer);
            SFFree(dwBlkpt);
            SFFree(buffer);
            return FALSE;
        }
        char *newbuffer = (char *)SFAlloc(fsz+ptsz);
        if (!newbuffer) {
            SFFree(outbuffer);
            SFFree(compbuffer);
            SFFree(dwBlkpt);
            SFFree(buffer);
            return FALSE;
        }
        DWORD CurPos=0;
        for (i=0;i<TotalBlocks;i++) {
            dwBlkpt[i] = LastOffset;
            if (i==TotalBlocks-1 && (ucfsz % blockSize)!=0) blockSize=ucfsz % blockSize;
            DWORD outLength=blockSize;
            BYTE *compdata = compbuffer;
            char *oldoutbuffer = outbuffer;
            if (dwFlags & MAFA_COMPRESS)
            {
                memcpy(compdata,(char *)buffer+CurPos,blockSize);
                if (i==0 && (dwCompressionType&0x40 || dwCompressionType&0x80)) {
                    if (SCompCompress(outbuffer, &outLength, compdata, blockSize, 0x08, 0, 0)==0)
                        outLength=blockSize;
                }
                else
                {
                    if (dwCompressionType&UNSUPPORTED_COMPRESSION) {
                        LoadStorm();
                        if (stormSCompCompress) {
                            if (stormSCompCompress(outbuffer, &outLength, compdata, blockSize, dwCompressionType, dwCompressionSubType, dwCompressLevel)==0)
                                outLength=blockSize;
                        }
                    }
                    else {
                        if (SCompCompress(outbuffer, &outLength, compdata, blockSize, dwCompressionType, dwCompressionSubType, dwCompressLevel)==0)
                            outLength=blockSize;
                    }
                }
            }
            else if (dwFlags & MAFA_COMPRESS2)
            {
                memcpy(compdata,(char *)buffer+CurPos,blockSize);
                if (SCompCompress(outbuffer, &outLength, compdata, blockSize, 0x08, dwCompressionSubType, 0)==0) {
                    outLength=blockSize;
                }
                else {
                    outbuffer++;
                    outLength--;
                }
            }
            else
            {
                memcpy(compdata,(char *)buffer+CurPos,blockSize);
            }
            if (outLength>=blockSize)
            {
                memcpy((char *)newbuffer+LastOffset,compdata,blockSize);
                LastOffset+=blockSize;
            }
            else {
                memcpy((char *)newbuffer+LastOffset,outbuffer,outLength);
                LastOffset+=outLength;
            }
            outbuffer = oldoutbuffer;
            CurPos += blockSize;
        }
        fsz = LastOffset;
        dwBlkpt[TotalBlocks] = fsz;
        memcpy(newbuffer,dwBlkpt,ptsz);

        SFFree(outbuffer);
        SFFree(compbuffer);
        memcpy(buffer,newbuffer,fsz);
        SFFree(newbuffer);
    }
    else
    {
        for (i=0;i<TotalBlocks+1;i++) {
            if (i<TotalBlocks) dwBlkpt[i] = i * blockSize;
            else dwBlkpt[i] = ucfsz;
        }
    }
    if (IsBNcache==TRUE)
    {
        buffer = hbuffer;
        fsz += 324;
    }
    HASHTABLEENTRY *hashEntry = (HASHTABLEENTRY *)hFile;
    BOOL IsNewBlockEntry = FALSE;DWORD OldBlockTableSize = sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize;
    if ((hashEntry->dwBlockTableIndex&0xFFFFFFFE)==0xFFFFFFFE)
    {
        BLOCKTABLEENTRY *lpnBlockTable;
        lpnBlockTable = (BLOCKTABLEENTRY *)SFAlloc(sizeof(BLOCKTABLEENTRY) * (mpqOpenArc->MpqHeader.dwBlockTableSize + 1));
        if(!lpnBlockTable) {
            SFFree(buffer);
            return FALSE;
        }
        if (mpqOpenArc->lpBlockTable) memcpy(lpnBlockTable,mpqOpenArc->lpBlockTable,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize);
        mpqOpenArc->MpqHeader.dwBlockTableSize++;
        if (mpqOpenArc->lpBlockTable) SFFree(mpqOpenArc->lpBlockTable);
        mpqOpenArc->lpBlockTable = lpnBlockTable;
        IsNewBlockEntry = TRUE;
    }
    DWORD BlockIndex = mpqOpenArc->MpqHeader.dwBlockTableSize - 1;
    if ((hashEntry->dwBlockTableIndex&0xFFFFFFFE)==0xFFFFFFFE)
    {
        hashEntry->dwNameHashA = HashString(lpDestFileName,HASH_NAME_A);
        hashEntry->dwNameHashB = HashString(lpDestFileName,HASH_NAME_B);
        hashEntry->lcLocale = LocaleID;
        hashEntry->dwBlockTableIndex = BlockIndex;
        mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset = 0;

    }
    else
    {
        BlockIndex = hashEntry->dwBlockTableIndex;
    }
    if (mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset==0 || mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize<fsz)
    {
        if (mpqOpenArc->MpqHeader.dwHashTableOffset+(mpqOpenArc->MpqHeader.dwHashTableSize * sizeof(HASHTABLEENTRY))+OldBlockTableSize==mpqOpenArc->MpqHeader.dwMPQSize)
        {
            if (mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize==mpqOpenArc->MpqHeader.dwHashTableOffset)
            {
                mpqOpenArc->MpqHeader.dwHashTableOffset += fsz-mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize;
                mpqOpenArc->MpqHeader.dwBlockTableOffset += fsz-mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize;
                mpqOpenArc->MpqHeader.dwMPQSize += fsz-mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
            else
            {
                mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset = mpqOpenArc->MpqHeader.dwHashTableOffset;
                mpqOpenArc->MpqHeader.dwHashTableOffset += fsz;
                mpqOpenArc->MpqHeader.dwBlockTableOffset += fsz;
                mpqOpenArc->MpqHeader.dwMPQSize += fsz;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
        }
        else if (mpqOpenArc->MpqHeader.dwBlockTableOffset+OldBlockTableSize==mpqOpenArc->MpqHeader.dwMPQSize)
        {
            if (mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize==mpqOpenArc->MpqHeader.dwBlockTableOffset)
            {
                mpqOpenArc->MpqHeader.dwBlockTableOffset += fsz-mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize;
                mpqOpenArc->MpqHeader.dwMPQSize += fsz-mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
            else
            {
                mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset = mpqOpenArc->MpqHeader.dwBlockTableOffset;
                mpqOpenArc->MpqHeader.dwBlockTableOffset += fsz;
                mpqOpenArc->MpqHeader.dwMPQSize += fsz;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
        }
        else
        {
            if (mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize==mpqOpenArc->MpqHeader.dwMPQSize)
            {
                mpqOpenArc->MpqHeader.dwBlockTableOffset = mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+fsz;
                mpqOpenArc->MpqHeader.dwMPQSize = mpqOpenArc->MpqHeader.dwBlockTableOffset+OldBlockTableSize;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
            else
            {
                mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset = mpqOpenArc->MpqHeader.dwMPQSize;
                mpqOpenArc->MpqHeader.dwBlockTableOffset = mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+fsz;
                mpqOpenArc->MpqHeader.dwMPQSize = mpqOpenArc->MpqHeader.dwBlockTableOffset+OldBlockTableSize;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
        }
    }
    mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize = fsz;
    mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize = ucfsz;
    mpqOpenArc->lpBlockTable[BlockIndex].dwFlags = dwFlags|MAFA_EXISTS;
    DWORD dwFileOffset = mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset;
    SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart,FILE_BEGIN);
    WriteFile(mpqOpenArc->hFile,&mpqOpenArc->MpqHeader,sizeof(MPQHEADER),&tsz,0);
    if (dwFlags & MAFA_ENCRYPT) {
        DWORD dwCryptKey;
        if (dwFlags&MAFA_ENCRYPT) dwCryptKey = HashString(lpDestFileName,HASH_KEY);
        if (dwFlags&MAFA_MODCRYPTKEY) dwCryptKey = (dwCryptKey + mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset) ^ mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize;
        DWORD blockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
        DWORD TotalBlocks = mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize / blockSize;
        if(mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize % blockSize)
            TotalBlocks++;
        DWORD dwBlockEnd = mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize % blockSize;
        if (dwBlockEnd==0) dwBlockEnd = blockSize;
        if ((dwFlags & MAFA_COMPRESS)==MAFA_COMPRESS || (dwFlags & MAFA_COMPRESS2)==MAFA_COMPRESS2) {
            EncryptData((LPBYTE)buffer,ptsz,dwCryptKey-1);
        }
        for (DWORD i=0;i<TotalBlocks;i++) {
            EncryptData((LPBYTE)buffer+dwBlkpt[i],dwBlkpt[i+1]-dwBlkpt[i],dwCryptKey);
            dwCryptKey++;
        }
    }
    SFFree(dwBlkpt);
    SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+dwFileOffset,FILE_BEGIN);
    WriteFile(mpqOpenArc->hFile,buffer,fsz,&tsz,0);
    SFFree(buffer);
    WriteHashTable(mpqOpenArc);
    WriteBlockTable(mpqOpenArc);
    AddToInternalListing(hMPQ,lpDestFileName);

    return TRUE;
}

BOOL SFMPQAPI WINAPI MpqAddFileToArchive(MPQHANDLE hMPQ, LPCSTR lpSourceFileName, LPCSTR lpDestFileName, DWORD dwFlags)
{
    return MpqAddFileToArchiveEx(hMPQ,lpSourceFileName,lpDestFileName,dwFlags,0x08,0);
}

BOOL SFMPQAPI WINAPI MpqAddWaveToArchive(MPQHANDLE hMPQ, LPCSTR lpSourceFileName, LPCSTR lpDestFileName, DWORD dwFlags, DWORD dwQuality)
{
    return MpqAddFileToArchiveEx(hMPQ,lpSourceFileName,lpDestFileName,dwFlags,0x81,dwQuality);
}

BOOL SFMPQAPI WINAPI MpqAddFileFromBufferEx(MPQHANDLE hMPQ, LPCVOID lpBuffer, DWORD dwLength, LPCSTR lpFileName, DWORD dwFlags, DWORD dwCompressionType, DWORD dwCompressLevel)
{
    if (!hMPQ || !lpBuffer || !lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!*lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!(((MPQARCHIVE *)hMPQ)->dwFlags&SFILE_OPEN_ALLOW_WRITE)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    BOOL ReplaceExisting;
    if (dwFlags&MAFA_REPLACE_EXISTING) {
        dwFlags = dwFlags^MAFA_REPLACE_EXISTING;
        ReplaceExisting = TRUE;

    }
    else ReplaceExisting = FALSE;
    MPQHANDLE hFile = GetFreeHashTableEntry(hMPQ,lpFileName,LocaleID,ReplaceExisting);
    if (hFile==0) return FALSE;
    DWORD fsz = dwLength,tsz;
    DWORD ucfsz = fsz;
    BOOL IsBNcache = FALSE;char *buffer = nullptr,*hbuffer = nullptr;
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    DWORD blockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
    DWORD TotalBlocks = fsz / blockSize;
    if(fsz % blockSize)
        TotalBlocks++;
    DWORD ptsz = 0;
    if (dwFlags&MAFA_COMPRESS || dwFlags&MAFA_COMPRESS2) ptsz = (TotalBlocks+1)*4;
    if (memcmp(&mpqOpenArc->MpqHeader.dwMPQID,ID_BN3,4)==0)
    {
        IsBNcache = TRUE;
        hbuffer = (char *)SFAlloc(fsz+324+ptsz);
        if (hbuffer==0) {
            return FALSE;
        }
        hbuffer[0] = (char)0x44;
        hbuffer[1] = (char)0x01;
        DWORD lpFileNameLength = strlen(lpFileName);
        if (lpFileNameLength<=260) memcpy(hbuffer+64,lpFileName,lpFileNameLength);
        else strncpy(hbuffer+64,lpFileName,260);
        buffer = hbuffer+324;
    }
    else buffer = (char *)SFAlloc(fsz+ptsz);
    if (buffer==0 && fsz!=0) {
        return FALSE;
    }
    if (fsz!=0) memcpy(buffer,lpBuffer,fsz);
    else {
        if (dwFlags&MAFA_COMPRESS) dwFlags = dwFlags^MAFA_COMPRESS;
        if (dwFlags&MAFA_COMPRESS2) dwFlags = dwFlags^MAFA_COMPRESS2;
        if (dwFlags&MAFA_ENCRYPT) dwFlags = dwFlags^MAFA_ENCRYPT;
        if (dwFlags&MAFA_MODCRYPTKEY) dwFlags = dwFlags^MAFA_MODCRYPTKEY;
    }
    DWORD i;
    DWORD *dwBlkpt = (DWORD *)SFAlloc((TotalBlocks+1)*4);

    if (dwBlkpt==0) {
        SFFree(buffer);
        return FALSE;
    }
    if ((dwFlags & MAFA_COMPRESS)==MAFA_COMPRESS || (dwFlags & MAFA_COMPRESS2)==MAFA_COMPRESS2)
    {
        DWORD dwCompressionSubType = 0;

        if ((dwCompressionType&0x40 || dwCompressionType&0x80) && (memcmp(buffer,ID_RIFF,4)!=0 || memcmp(buffer+8,ID_WAVE,8)!=0 || buffer[20]!=1 || buffer[34]!=16)) {
            dwCompressionType = 0x08;
            dwCompressLevel = 0;
        }
        if (dwCompressionType&0x40 || dwCompressionType&0x80) {
            switch (dwCompressLevel) {
                case MAWA_QUALITY_HIGH:
                    dwCompressLevel = 1;
                    break;
                case MAWA_QUALITY_LOW:
                    dwCompressLevel = 3;
                    break;
                default:
                    dwCompressLevel = 0;
            }
        }
        else if (dwCompressionType&0x01 || dwCompressionType&0x08 || ((dwFlags & MAFA_COMPRESS)!=MAFA_COMPRESS && (dwFlags & MAFA_COMPRESS2)==MAFA_COMPRESS2)) {
            dwCompressionSubType = dwCompressLevel;
            dwCompressLevel = 0;
        }
        else if (dwCompressionType&0x02) {
            dwCompressionSubType = 1;
        }

        DWORD LastOffset=ptsz;
        BYTE *compbuffer = (BYTE *)SFAlloc(blockSize);
        if (compbuffer==0) {
            SFFree(dwBlkpt);
            SFFree(buffer);
            return FALSE;
        }

        char *outbuffer = (char *)SFAlloc(blockSize);
        if (outbuffer==0) {
            SFFree(compbuffer);
            SFFree(dwBlkpt);
            SFFree(buffer);
            return FALSE;
        }
        char *newbuffer = (char *)SFAlloc(fsz+ptsz);
        if (newbuffer==0) {
            SFFree(outbuffer);
            SFFree(compbuffer);
            SFFree(dwBlkpt);
            SFFree(buffer);
            return FALSE;
        }
        DWORD CurPos=0;
        for (i=0;i<TotalBlocks;i++) {
            dwBlkpt[i] = LastOffset;
            if (i==TotalBlocks-1 && (ucfsz % blockSize)!=0) blockSize=ucfsz % blockSize;
            DWORD outLength=blockSize;
            BYTE *compdata = compbuffer;
            char *oldoutbuffer = outbuffer;
            if (dwFlags & MAFA_COMPRESS)
            {
                memcpy(compdata,(char *)buffer+CurPos,blockSize);
                if (i==0 && (dwCompressionType&0x40 || dwCompressionType&0x80)) {
                    if (SCompCompress(outbuffer, &outLength, compdata, blockSize, 0x08, 0, 0)==0)
                        outLength=blockSize;
                }
                else
                {
                    if (dwCompressionType&UNSUPPORTED_COMPRESSION) {
                        LoadStorm();
                        if (stormSCompCompress!=0) {
                            if (stormSCompCompress(outbuffer, &outLength, compdata, blockSize, dwCompressionType, dwCompressionSubType, dwCompressLevel)==0)
                                outLength=blockSize;
                        }
                    }
                    else {
                        if (SCompCompress(outbuffer, &outLength, compdata, blockSize, dwCompressionType, dwCompressionSubType, dwCompressLevel)==0)
                            outLength=blockSize;
                    }
                }
            }
            else if (dwFlags & MAFA_COMPRESS2)
            {
                memcpy(compdata,(char *)buffer+CurPos,blockSize);
                if (SCompCompress(outbuffer, &outLength, compdata, blockSize, 0x08, dwCompressionSubType, 0)==0) {
                    outLength=blockSize;
                }
                else {
                    outbuffer++;
                    outLength--;
                }
            }
            else
            {
                memcpy(compdata,(char *)buffer+CurPos,blockSize);
            }
            if (outLength>=blockSize)
            {
                memcpy((char *)newbuffer+LastOffset,compdata,blockSize);
                LastOffset+=blockSize;
            }
            else {
                memcpy((char *)newbuffer+LastOffset,outbuffer,outLength);
                LastOffset+=outLength;
            }
            outbuffer = oldoutbuffer;
            CurPos += blockSize;
        }
        fsz = LastOffset;
        dwBlkpt[TotalBlocks] = fsz;
        memcpy(newbuffer,dwBlkpt,ptsz);
        SFFree(outbuffer);
        SFFree(compbuffer);
        memcpy(buffer,newbuffer,fsz);
        SFFree(newbuffer);
    }
    else
    {
        for (i=0;i<TotalBlocks+1;i++) {
            if (i<TotalBlocks) dwBlkpt[i] = i * blockSize;
            else dwBlkpt[i] = ucfsz;
        }
    }
    if (IsBNcache==TRUE)
    {
        buffer = hbuffer;
        fsz += 324;
    }
    HASHTABLEENTRY *hashEntry = (HASHTABLEENTRY *)hFile;

    BOOL IsNewBlockEntry = FALSE;DWORD OldBlockTableSize = sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize;
    if ((hashEntry->dwBlockTableIndex&0xFFFFFFFE)==0xFFFFFFFE)
    {
        BLOCKTABLEENTRY *lpnBlockTable;
        lpnBlockTable = (BLOCKTABLEENTRY *)SFAlloc(sizeof(BLOCKTABLEENTRY) * (mpqOpenArc->MpqHeader.dwBlockTableSize + 1));
        if(lpnBlockTable==0) {
            SFFree(buffer);
            return FALSE;
        }
        if (mpqOpenArc->lpBlockTable!=0) memcpy(lpnBlockTable,mpqOpenArc->lpBlockTable,sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize);
        mpqOpenArc->MpqHeader.dwBlockTableSize++;
        if (mpqOpenArc->lpBlockTable!=0) SFFree(mpqOpenArc->lpBlockTable);
        mpqOpenArc->lpBlockTable = lpnBlockTable;
        IsNewBlockEntry = TRUE;
    }
    DWORD BlockIndex = mpqOpenArc->MpqHeader.dwBlockTableSize - 1;
    if ((hashEntry->dwBlockTableIndex&0xFFFFFFFE)==0xFFFFFFFE)
    {
        hashEntry->dwNameHashA = HashString(lpFileName,HASH_NAME_A);
        hashEntry->dwNameHashB = HashString(lpFileName,HASH_NAME_B);
        hashEntry->lcLocale = LocaleID;
        hashEntry->dwBlockTableIndex = BlockIndex;
        mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset = 0;
    }
    else
    {
        BlockIndex = hashEntry->dwBlockTableIndex;
    }
    if (mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset==0 || mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize<fsz)
    {
        if (mpqOpenArc->MpqHeader.dwHashTableOffset+(mpqOpenArc->MpqHeader.dwHashTableSize * sizeof(HASHTABLEENTRY))+OldBlockTableSize==mpqOpenArc->MpqHeader.dwMPQSize)
        {
            if (mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize==mpqOpenArc->MpqHeader.dwHashTableOffset)
            {
                mpqOpenArc->MpqHeader.dwHashTableOffset += fsz-mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize;
                mpqOpenArc->MpqHeader.dwBlockTableOffset += fsz-mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize;
                mpqOpenArc->MpqHeader.dwMPQSize += fsz-mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
            else
            {
                mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset = mpqOpenArc->MpqHeader.dwHashTableOffset;
                mpqOpenArc->MpqHeader.dwHashTableOffset += fsz;
                mpqOpenArc->MpqHeader.dwBlockTableOffset += fsz;
                mpqOpenArc->MpqHeader.dwMPQSize += fsz;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
        }
        else if (mpqOpenArc->MpqHeader.dwBlockTableOffset+OldBlockTableSize==mpqOpenArc->MpqHeader.dwMPQSize)
        {
            if (mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize==mpqOpenArc->MpqHeader.dwBlockTableOffset)
            {
                mpqOpenArc->MpqHeader.dwBlockTableOffset += fsz-mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize;
                mpqOpenArc->MpqHeader.dwMPQSize += fsz-mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
            else
            {
                mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset = mpqOpenArc->MpqHeader.dwBlockTableOffset;
                mpqOpenArc->MpqHeader.dwBlockTableOffset += fsz;
                mpqOpenArc->MpqHeader.dwMPQSize += fsz;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
        }
        else
        {
            if (mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize==mpqOpenArc->MpqHeader.dwMPQSize)
            {
                mpqOpenArc->MpqHeader.dwBlockTableOffset = mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+fsz;
                mpqOpenArc->MpqHeader.dwMPQSize = mpqOpenArc->MpqHeader.dwBlockTableOffset+OldBlockTableSize;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
            else
            {
                mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset = mpqOpenArc->MpqHeader.dwMPQSize;
                mpqOpenArc->MpqHeader.dwBlockTableOffset = mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+fsz;
                mpqOpenArc->MpqHeader.dwMPQSize = mpqOpenArc->MpqHeader.dwBlockTableOffset+OldBlockTableSize;
                if (IsNewBlockEntry==TRUE) mpqOpenArc->MpqHeader.dwMPQSize += sizeof(BLOCKTABLEENTRY);
            }
        }
    }
    mpqOpenArc->lpBlockTable[BlockIndex].dwCompressedSize = fsz;
    mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize = ucfsz;
    mpqOpenArc->lpBlockTable[BlockIndex].dwFlags = dwFlags|MAFA_EXISTS;
    DWORD dwFileOffset = mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset;
    SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart,FILE_BEGIN);
    WriteFile(mpqOpenArc->hFile,&mpqOpenArc->MpqHeader,sizeof(MPQHEADER),&tsz,0);
    if (dwFlags & MAFA_ENCRYPT) {
        DWORD dwCryptKey;
        if (dwFlags&MAFA_ENCRYPT) dwCryptKey = HashString(lpFileName,HASH_KEY);
        if (dwFlags&MAFA_MODCRYPTKEY) dwCryptKey = (dwCryptKey + mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset) ^ mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize;
        DWORD blockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
        DWORD TotalBlocks = mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize / blockSize;

        if(mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize % blockSize)
            TotalBlocks++;
        DWORD dwBlockEnd = mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize % blockSize;
        if (dwBlockEnd==0) dwBlockEnd = blockSize;
        if ((dwFlags & MAFA_COMPRESS)==MAFA_COMPRESS || (dwFlags & MAFA_COMPRESS2)==MAFA_COMPRESS2) {
            EncryptData((LPBYTE)buffer,ptsz,dwCryptKey-1);
        }
        for (DWORD i=0;i<TotalBlocks;i++) {
            EncryptData((LPBYTE)buffer+dwBlkpt[i],dwBlkpt[i+1]-dwBlkpt[i],dwCryptKey);
            dwCryptKey++;
        }
    }
    SFFree(dwBlkpt);
    SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+dwFileOffset,FILE_BEGIN);
    WriteFile(mpqOpenArc->hFile,buffer,fsz,&tsz,0);
    SFFree(buffer);
    WriteHashTable(mpqOpenArc);
    WriteBlockTable(mpqOpenArc);
    AddToInternalListing(hMPQ,lpFileName);
    return TRUE;
}

BOOL SFMPQAPI WINAPI MpqAddFileFromBuffer(MPQHANDLE hMPQ, LPCVOID lpBuffer, DWORD dwLength, LPCSTR lpFileName, DWORD dwFlags)
{
    return MpqAddFileFromBufferEx(hMPQ,lpBuffer,dwLength,lpFileName,dwFlags,0x08,0);
}

BOOL SFMPQAPI WINAPI MpqAddWaveFromBuffer(MPQHANDLE hMPQ, LPVOID lpBuffer, DWORD dwLength, LPCSTR lpFileName, DWORD dwFlags, DWORD dwQuality)
{
    return MpqAddFileFromBufferEx(hMPQ,lpBuffer,dwLength,lpFileName,dwFlags,0x81,dwQuality);
}

BOOL SFMPQAPI WINAPI MpqRenameFile(MPQHANDLE hMPQ, LPCSTR lpcOldFileName, LPCSTR lpcNewFileName)
{
    return MpqRenameAndSetFileLocale(hMPQ,lpcOldFileName,lpcNewFileName,LocaleID,LocaleID);
}

BOOL SFMPQAPI WINAPI MpqRenameAndSetFileLocale(MPQHANDLE hMPQ, LPCSTR lpcOldFileName, LPCSTR lpcNewFileName, LCID nOldLocale, LCID nNewLocale)
{
    if (!hMPQ || !lpcOldFileName || !lpcNewFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!*lpcNewFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!(((MPQARCHIVE *)hMPQ)->dwFlags&SFILE_OPEN_ALLOW_WRITE)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    if (_stricmp(lpcOldFileName,lpcNewFileName)==0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    HASHTABLEENTRY *oldHashEntry = (HASHTABLEENTRY *)GetHashTableEntry(hMPQ,lpcOldFileName,nOldLocale);
    if (oldHashEntry==0) {
        SetLastError(MPQ_ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    if (oldHashEntry->lcLocale!=nOldLocale) {
        SetLastError(MPQ_ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    HASHTABLEENTRY *newHashEntry = (HASHTABLEENTRY *)GetFreeHashTableEntry(hMPQ,lpcNewFileName,nNewLocale,TRUE);
    if ((newHashEntry->dwBlockTableIndex&0xFFFFFFFE)!=0xFFFFFFFE)
        return FALSE;
    if (newHashEntry==0) newHashEntry = oldHashEntry;
    DWORD tsz;
    newHashEntry->dwNameHashA = HashString(lpcNewFileName,HASH_NAME_A);
    newHashEntry->dwNameHashB = HashString(lpcNewFileName,HASH_NAME_B);
    newHashEntry->lcLocale = nNewLocale;
    newHashEntry->dwBlockTableIndex = oldHashEntry->dwBlockTableIndex;
    DWORD BlockIndex = oldHashEntry->dwBlockTableIndex;
    if (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_ENCRYPT)
    {
        DWORD dwOldCryptKey = HashString(lpcOldFileName,HASH_KEY);
        DWORD dwNewCryptKey = HashString(lpcNewFileName,HASH_KEY);
        if (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_MODCRYPTKEY) {
            dwOldCryptKey = (dwOldCryptKey + mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset) ^ mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize;

            dwNewCryptKey = (dwNewCryptKey + mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset) ^ mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize;
        }
        if (dwOldCryptKey!=dwNewCryptKey)
        {
            DWORD HeaderLength=0;
            if (memcmp(&mpqOpenArc->MpqHeader.dwMPQID,ID_BN3,4)==0)
            {
                SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset,FILE_BEGIN);
                ReadFile(mpqOpenArc->hFile,&HeaderLength,4,&tsz,0);

            }
            DWORD blockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
            DWORD TotalBlocks = mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize / blockSize;
            if(mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize % blockSize)
                TotalBlocks++;
            DWORD *dwBlockPtrTable = (DWORD *)SFAlloc((TotalBlocks+1)*4);
            if (dwBlockPtrTable==0) {
                return FALSE;
            }
            DWORD i;
            if ((mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_COMPRESS)==MAFA_COMPRESS || (mpqOpenArc->lpBlockTable[BlockIndex].dwFlags & MAFA_COMPRESS2)==MAFA_COMPRESS2)
            {
                SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+HeaderLength,FILE_BEGIN);
                ReadFile(mpqOpenArc->hFile,dwBlockPtrTable,(TotalBlocks+1)*4,&tsz,0);
                DecryptData((LPBYTE)dwBlockPtrTable,(TotalBlocks+1)*4,dwOldCryptKey-1);
                char *EncryptedTable = (char *)SFAlloc((TotalBlocks+1)*4);
                if (EncryptedTable==0) {
                    SFFree(dwBlockPtrTable);
                    return FALSE;
                }
                memcpy(EncryptedTable,dwBlockPtrTable,(TotalBlocks+1)*4);
                EncryptData((LPBYTE)EncryptedTable,(TotalBlocks+1)*4,dwNewCryptKey-1);
                SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+HeaderLength,FILE_BEGIN);
                WriteFile(mpqOpenArc->hFile,EncryptedTable,(TotalBlocks+1)*4,&tsz,0);
                SFFree(EncryptedTable);
            }
            else
            {
                for (i=0;i<TotalBlocks+1;i++) {
                    if (i<TotalBlocks) dwBlockPtrTable[i] = i * blockSize;
                    else dwBlockPtrTable[i] = mpqOpenArc->lpBlockTable[BlockIndex].dwFullSize;
                }
            }
            char *blkBuffer = (char *)SFAlloc(blockSize);
            if (blkBuffer==0) {
                EncryptData((LPBYTE)dwBlockPtrTable,(TotalBlocks+1)*4,dwOldCryptKey-1);
                SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+HeaderLength,FILE_BEGIN);
                WriteFile(mpqOpenArc->hFile,dwBlockPtrTable,(TotalBlocks+1)*4,&tsz,0);
                SFFree(dwBlockPtrTable);
                return FALSE;
            }
            for (i=0;i<TotalBlocks;i++) {
                SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+HeaderLength+dwBlockPtrTable[i],FILE_BEGIN);
                if (ReadFile(mpqOpenArc->hFile,blkBuffer,dwBlockPtrTable[i+1]-dwBlockPtrTable[i],&tsz,0)==0) {
                    EncryptData((LPBYTE)dwBlockPtrTable,(TotalBlocks+1)*4,dwOldCryptKey-1);
                    SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+HeaderLength,FILE_BEGIN);
                    WriteFile(mpqOpenArc->hFile,dwBlockPtrTable,(TotalBlocks+1)*4,&tsz,0);
                    SFFree(dwBlockPtrTable);
                    SFFree(blkBuffer);
                    return FALSE;
                }
                DecryptData((LPBYTE)blkBuffer,dwBlockPtrTable[i+1]-dwBlockPtrTable[i],dwOldCryptKey+i);
                EncryptData((LPBYTE)blkBuffer,dwBlockPtrTable[i+1]-dwBlockPtrTable[i],dwNewCryptKey+i);
                SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[BlockIndex].dwFileOffset+HeaderLength+dwBlockPtrTable[i],FILE_BEGIN);
                WriteFile(mpqOpenArc->hFile,blkBuffer,dwBlockPtrTable[i+1]-dwBlockPtrTable[i],&tsz,0);
            }
            SFFree(dwBlockPtrTable);
            SFFree(blkBuffer);
        }
    }
    if (oldHashEntry!=newHashEntry) {
        oldHashEntry->dwNameHashA = 0xFFFFFFFF;
        oldHashEntry->dwNameHashB = 0xFFFFFFFF;
        oldHashEntry->lcLocale = 0xFFFFFFFF;
        oldHashEntry->dwBlockTableIndex = 0xFFFFFFFE;
        WriteHashTable(mpqOpenArc);
    }
    LCID dwOldLocale=LocaleID;
    LocaleID=nOldLocale;
    RemoveFromInternalListing(hMPQ,lpcOldFileName);

    LocaleID=nNewLocale;
    AddToInternalListing(hMPQ,lpcNewFileName);
    LocaleID=dwOldLocale;
    return TRUE;
}

BOOL SFMPQAPI WINAPI MpqDeleteFile(MPQHANDLE hMPQ, LPCSTR lpFileName)
{
    return MpqDeleteFileWithLocale(hMPQ,lpFileName,LocaleID);
}

BOOL SFMPQAPI WINAPI MpqDeleteFileWithLocale(MPQHANDLE hMPQ, LPCSTR lpFileName, LCID nLocale)
{
    if (!hMPQ || !lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!(((MPQARCHIVE *)hMPQ)->dwFlags&SFILE_OPEN_ALLOW_WRITE)) {

        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    HASHTABLEENTRY *hashEntry = (HASHTABLEENTRY *)GetHashTableEntry(hMPQ,lpFileName,nLocale);
    if (hashEntry==0) {
        SetLastError(MPQ_ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    if (hashEntry->lcLocale!=nLocale) return FALSE;
    hashEntry->dwNameHashA = 0xFFFFFFFF;
    hashEntry->dwNameHashB = 0xFFFFFFFF;
    hashEntry->lcLocale = 0xFFFFFFFF;
    hashEntry->dwBlockTableIndex = 0xFFFFFFFE;
    WriteHashTable(mpqOpenArc);
    LCID dwOldLocale=LocaleID;
    LocaleID=nLocale;
    RemoveFromInternalListing(hMPQ,lpFileName);
    LocaleID=dwOldLocale;
    return TRUE;
}

BOOL SFMPQAPI WINAPI MpqCompactArchive(MPQHANDLE hMPQ)
{
    if (!hMPQ) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!(((MPQARCHIVE *)hMPQ)->dwFlags&SFILE_OPEN_ALLOW_WRITE)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    TempAlloc NewAlloc;
    char *lpFileName = (char *)NewAlloc.Alloc(strlen(mpqOpenArc->lpFileName)+13);
    sprintf(lpFileName,"%s.compact",mpqOpenArc->lpFileName);
    HANDLE hFile = CreateFile(lpFileName,GENERIC_READ|GENERIC_WRITE,0,0,CREATE_NEW,0,0);
    DWORD i;
    if (hFile==INVALID_HANDLE_VALUE) {
        for (i=0;i<10000;i++) {
            sprintf(lpFileName,"%s.compact.%04d",mpqOpenArc->lpFileName,i);

            hFile = CreateFile(lpFileName,GENERIC_READ|GENERIC_WRITE,0,0,CREATE_NEW,0,0);
            if (hFile!=INVALID_HANDLE_VALUE) break;
        }
        if (i==10000) return FALSE;
    }
    DWORD dwLastOffset = sizeof(MPQHEADER),tsz;
    char *buffer = (char *)SFAlloc(65536);
    if (buffer==0) {
        CloseHandle(hFile);
        DeleteFile(lpFileName);
        return FALSE;
    }
    HASHTABLEENTRY *lpHashTable = (HASHTABLEENTRY *)SFAlloc(sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize);
    if (lpHashTable==0) {
        SFFree(buffer);
        CloseHandle(hFile);
        DeleteFile(lpFileName);
        return FALSE;
    }
    BLOCKTABLEENTRY *lpBlockTable = (BLOCKTABLEENTRY *)SFAlloc(sizeof(BLOCKTABLEENTRY) * mpqOpenArc->MpqHeader.dwBlockTableSize);
    if(mpqOpenArc->lpBlockTable!=0) {
        if (lpBlockTable==0) {
            SFFree(lpHashTable);
            SFFree(buffer);
            CloseHandle(hFile);
            DeleteFile(lpFileName);

            return FALSE;
        }
    }
    DWORD j=0,nBlkOffset=0,ReadSize=0;
    memcpy(lpHashTable,mpqOpenArc->lpHashTable,sizeof(HASHTABLEENTRY) * mpqOpenArc->MpqHeader.dwHashTableSize);
    for (i=0;i<mpqOpenArc->MpqHeader.dwBlockTableSize;i++) {
        for (j=0;j<mpqOpenArc->MpqHeader.dwHashTableSize;j++) {
            if (lpHashTable[j].dwBlockTableIndex==(i-nBlkOffset)) break;
        }
        if (j<mpqOpenArc->MpqHeader.dwHashTableSize) {
            memcpy(&lpBlockTable[i-nBlkOffset],&mpqOpenArc->lpBlockTable[i],sizeof(BLOCKTABLEENTRY));
            lpBlockTable[i-nBlkOffset].dwFileOffset = dwLastOffset;
            dwLastOffset += mpqOpenArc->lpBlockTable[i].dwCompressedSize;
            DWORD dwWritten=FALSE;
            if (mpqOpenArc->lpBlockTable[i].dwFlags&MAFA_ENCRYPT && mpqOpenArc->lpBlockTable[i].dwFlags&MAFA_MODCRYPTKEY && (mpqOpenArc->lpBlockTable[i].dwFlags&MAFA_COMPRESS || mpqOpenArc->lpBlockTable[i].dwFlags&MAFA_COMPRESS2))
            {
                DWORD HeaderLength=0;
                if (memcmp(&mpqOpenArc->MpqHeader.dwMPQID,ID_BN3,4)==0)
                {
                    SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[i].dwFileOffset,FILE_BEGIN);
                    ReadFile(mpqOpenArc->hFile,&HeaderLength,4,&tsz,0);
                }
                DWORD blockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
                DWORD TotalBlocks = mpqOpenArc->lpBlockTable[i].dwFullSize / blockSize;
                if(mpqOpenArc->lpBlockTable[i].dwFullSize % blockSize)
                    TotalBlocks++;
                DWORD *dwBlockPtrTable = (DWORD *)SFAlloc((TotalBlocks+1)*4);
                if (dwBlockPtrTable==0) {
                    SFFree(lpBlockTable);
                    SFFree(lpHashTable);
                    SFFree(buffer);
                    CloseHandle(hFile);
                    DeleteFile(lpFileName);
                    return FALSE;
                }
                SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[i].dwFileOffset+HeaderLength,FILE_BEGIN);
                ReadFile(mpqOpenArc->hFile,dwBlockPtrTable,(TotalBlocks+1)*4,&tsz,0);
                DWORD dwOldCryptKey = DetectFileSeed(dwBlockPtrTable,(TotalBlocks+1)*4,blockSize);
                DWORD dwNewCryptKey = (dwOldCryptKey ^ mpqOpenArc->lpBlockTable[i].dwFullSize) - mpqOpenArc->lpBlockTable[i].dwFileOffset;
                dwNewCryptKey = (dwNewCryptKey + lpBlockTable[i-nBlkOffset].dwFileOffset) ^ lpBlockTable[i-nBlkOffset].dwFullSize;
                if (dwOldCryptKey==0) {
                    DWORD dwNameHashA = HashString(INTERNAL_LISTFILE,HASH_NAME_A);
                    DWORD dwNameHashB = HashString(INTERNAL_LISTFILE,HASH_NAME_B);
                    if (lpHashTable[j].dwNameHashA==dwNameHashA && lpHashTable[j].dwNameHashB==dwNameHashB) {
                        dwOldCryptKey = HashString(INTERNAL_LISTFILE,HASH_KEY);
                        dwNewCryptKey = dwOldCryptKey;
                        dwOldCryptKey = (dwOldCryptKey + mpqOpenArc->lpBlockTable[i].dwFileOffset) ^ mpqOpenArc->lpBlockTable[i].dwFullSize;
                        dwNewCryptKey = (dwNewCryptKey + lpBlockTable[i-nBlkOffset].dwFileOffset) ^ lpBlockTable[i-nBlkOffset].dwFullSize;
                    }
                    else {
                        HANDLE hlFile;
                        DWORD fsz;
                        char *listbuffer = nullptr;
                        LCID lcOldLocale = LocaleID;
                        for (DWORD lcn=0;lcn<nLocales;lcn++) {
                            LocaleID = availLocales[lcn];
                            if (SFileOpenFileEx(hMPQ,INTERNAL_LISTFILE,0,&hlFile)!=0) {
                                if (((MPQFILE *)hlFile)->lpHashEntry->lcLocale==0 && LocaleID!=0) {

                                    SFileCloseFile(hlFile);
                                    continue;
                                }
                                fsz = SFileGetFileSize(hlFile,0);
                                if (fsz>0) {
                                    listbuffer = (char *)SFAlloc(fsz+1);
                                    if (listbuffer==0) {
                                        SFileCloseFile(hlFile);
                                        continue;
                                    }
                                    if (SFileReadFile(hlFile,listbuffer,fsz,0,0)==0) {
                                        SFFree(listbuffer);
                                        listbuffer = 0;
                                    }
                                }
                                SFileCloseFile(hlFile);
                                if (listbuffer!=0) {
                                    char *listline;
                                    for (listline=listbuffer;listline!=0;listline=nextline(listline)) {
                                        if (listline[0]==0) break;
                                        DWORD lnlen=strlnlen(listline);
                                        char prevchar=listline[lnlen];
                                        listline[lnlen]=0;
                                        dwNameHashA = HashString(listline,HASH_NAME_A);
                                        dwNameHashB = HashString(listline,HASH_NAME_B);
                                        if (lpHashTable[j].dwNameHashA==dwNameHashA && lpHashTable[j].dwNameHashB==dwNameHashB) {
                                            dwOldCryptKey = HashString(listline,HASH_KEY);
                                            dwNewCryptKey = dwOldCryptKey;
                                            dwOldCryptKey = (dwOldCryptKey + mpqOpenArc->lpBlockTable[i].dwFileOffset) ^ mpqOpenArc->lpBlockTable[i].dwFullSize;
                                            dwNewCryptKey = (dwNewCryptKey + lpBlockTable[i-nBlkOffset].dwFileOffset) ^ lpBlockTable[i-nBlkOffset].dwFullSize;
                                            break;
                                        }
                                        listline[lnlen]=prevchar;
                                    }
                                    if (listline!=0) {
                                        if (listline[0]!=0) {
                                            SFFree(listbuffer);
                                            break;
                                        }
                                    }
                                    SFFree(listbuffer);
                                }
                            }
                        }
                        LocaleID = lcOldLocale;
                    }
                }
                if (dwOldCryptKey!=dwNewCryptKey)
                {
                    DecryptData((LPBYTE)dwBlockPtrTable,(TotalBlocks+1)*4,dwOldCryptKey-1);
                    char *EncryptedTable = (char *)SFAlloc((TotalBlocks+1)*4);
                    if (EncryptedTable==0) {
                        SFFree(dwBlockPtrTable);
                        SFFree(lpBlockTable);
                        SFFree(lpHashTable);
                        SFFree(buffer);
                        CloseHandle(hFile);
                        DeleteFile(lpFileName);
                        return FALSE;
                    }
                    memcpy(EncryptedTable,dwBlockPtrTable,(TotalBlocks+1)*4);
                    EncryptData((LPBYTE)EncryptedTable,(TotalBlocks+1)*4,dwNewCryptKey-1);
                    SFSetFilePointer(hFile,lpBlockTable[i-nBlkOffset].dwFileOffset+HeaderLength,FILE_BEGIN);
                    WriteFile(hFile,EncryptedTable,(TotalBlocks+1)*4,&tsz,0);
                    SFFree(EncryptedTable);
                    char *blkBuffer = (char *)SFAlloc(blockSize);
                    if (blkBuffer==0) {
                        SFFree(dwBlockPtrTable);
                        SFFree(lpBlockTable);
                        SFFree(lpHashTable);
                        SFFree(buffer);
                        CloseHandle(hFile);
                        DeleteFile(lpFileName);
                        return FALSE;
                    }
                    for (DWORD k=0;k<TotalBlocks;k++) {
                        SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[i].dwFileOffset+HeaderLength+dwBlockPtrTable[k],FILE_BEGIN);
                        if (ReadFile(mpqOpenArc->hFile,blkBuffer,dwBlockPtrTable[k+1]-dwBlockPtrTable[k],&tsz,0)==0) {
                            SFFree(dwBlockPtrTable);
                            SFFree(blkBuffer);
                            SFFree(lpBlockTable);
                            SFFree(lpHashTable);
                            SFFree(buffer);
                            CloseHandle(hFile);
                            DeleteFile(lpFileName);
                            return FALSE;
                        }
                        DecryptData((LPBYTE)blkBuffer,dwBlockPtrTable[k+1]-dwBlockPtrTable[k],dwOldCryptKey+k);
                        EncryptData((LPBYTE)blkBuffer,dwBlockPtrTable[k+1]-dwBlockPtrTable[k],dwNewCryptKey+k);
                        SFSetFilePointer(hFile,lpBlockTable[i-nBlkOffset].dwFileOffset+HeaderLength+dwBlockPtrTable[k],FILE_BEGIN);
                        WriteFile(hFile,blkBuffer,dwBlockPtrTable[k+1]-dwBlockPtrTable[k],&tsz,0);
                    }
                    SFFree(blkBuffer);
                    dwWritten = TRUE;
                }
                SFFree(dwBlockPtrTable);
            }
            else if (mpqOpenArc->lpBlockTable[i].dwFlags&MAFA_ENCRYPT && mpqOpenArc->lpBlockTable[i].dwFlags&MAFA_MODCRYPTKEY)
            {
                DWORD HeaderLength=0;
                if (memcmp(&mpqOpenArc->MpqHeader.dwMPQID,ID_BN3,4)==0)

                {
                    SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[i].dwFileOffset,FILE_BEGIN);
                    ReadFile(mpqOpenArc->hFile,&HeaderLength,4,&tsz,0);
                }
                DWORD blockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
                DWORD TotalBlocks = mpqOpenArc->lpBlockTable[i].dwFullSize / blockSize;
                if(mpqOpenArc->lpBlockTable[i].dwFullSize % blockSize)
                    TotalBlocks++;
                DWORD dwNameHashA = HashString(INTERNAL_LISTFILE,HASH_NAME_A);
                DWORD dwNameHashB = HashString(INTERNAL_LISTFILE,HASH_NAME_B);
                DWORD dwOldCryptKey=0;
                DWORD dwNewCryptKey=0;
                if (lpHashTable[j].dwNameHashA==dwNameHashA && lpHashTable[j].dwNameHashB==dwNameHashB) {
                    dwOldCryptKey = HashString(INTERNAL_LISTFILE,HASH_KEY);
                    dwNewCryptKey = dwOldCryptKey;
                    dwOldCryptKey = (dwOldCryptKey + mpqOpenArc->lpBlockTable[i].dwFileOffset) ^ mpqOpenArc->lpBlockTable[i].dwFullSize;
                    dwNewCryptKey = (dwNewCryptKey + lpBlockTable[i-nBlkOffset].dwFileOffset) ^ lpBlockTable[i-nBlkOffset].dwFullSize;
                }
                else {
                    HANDLE hlFile;
                    DWORD fsz;

                    char *listbuffer = nullptr;
                    LCID lcOldLocale = LocaleID;
                    for (DWORD lcn=0;lcn<nLocales;lcn++) {
                        LocaleID = availLocales[lcn];
                        if (SFileOpenFileEx(hMPQ,INTERNAL_LISTFILE,0,&hlFile)!=0) {
                            if (((MPQFILE *)hlFile)->lpHashEntry->lcLocale==0 && LocaleID!=0) {
                                SFileCloseFile(hlFile);
                                continue;
                            }
                            fsz = SFileGetFileSize(hlFile,0);
                            if (fsz>0) {
                                listbuffer = (char *)SFAlloc(fsz+1);
                                if (listbuffer==0) {
                                    SFileCloseFile(hlFile);
                                    continue;
                                }
                                if (SFileReadFile(hlFile,listbuffer,fsz,0,0)==0) {
                                    SFFree(listbuffer);
                                    listbuffer = 0;
                                }
                            }
                            SFileCloseFile(hlFile);
                            if (listbuffer!=0) {
                                char *listline;
                                for (listline=listbuffer;listline!=0;listline=nextline(listline)) {
                                    if (listline[0]==0) break;
                                    DWORD lnlen=strlnlen(listline);
                                    char prevchar=listline[lnlen];
                                    listline[lnlen]=0;
                                    dwNameHashA = HashString(listline,HASH_NAME_A);
                                    dwNameHashB = HashString(listline,HASH_NAME_B);
                                    if (lpHashTable[j].dwNameHashA==dwNameHashA && lpHashTable[j].dwNameHashB==dwNameHashB) {
                                        dwOldCryptKey = HashString(listline,HASH_KEY);
                                        dwNewCryptKey = dwOldCryptKey;
                                        dwOldCryptKey = (dwOldCryptKey + mpqOpenArc->lpBlockTable[i].dwFileOffset) ^ mpqOpenArc->lpBlockTable[i].dwFullSize;
                                        dwNewCryptKey = (dwNewCryptKey + lpBlockTable[i-nBlkOffset].dwFileOffset) ^ lpBlockTable[i-nBlkOffset].dwFullSize;
                                        break;
                                    }
                                    listline[lnlen]=prevchar;
                                }
                                if (listline!=0) {
                                    if (listline[0]!=0) {
                                        SFFree(listbuffer);
                                        break;
                                    }
                                }
                                SFFree(listbuffer);
                            }
                        }
                    }
                    LocaleID = lcOldLocale;
                }
                if (dwOldCryptKey!=dwNewCryptKey)
                {
                    char *blkBuffer = (char *)SFAlloc(blockSize);
                    if (blkBuffer==0) {
                        SFFree(lpBlockTable);
                        SFFree(lpHashTable);
                        SFFree(buffer);
                        CloseHandle(hFile);
                        DeleteFile(lpFileName);
                        return FALSE;
                    }
                    for (DWORD k=0;k<mpqOpenArc->lpBlockTable[i].dwFullSize;k+=blockSize) {
                        SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[i].dwFileOffset+HeaderLength+k,FILE_BEGIN);
                        if (k+blockSize>mpqOpenArc->lpBlockTable[i].dwFullSize) blockSize = mpqOpenArc->lpBlockTable[i].dwFullSize % blockSize;
                        if (ReadFile(mpqOpenArc->hFile,blkBuffer,blockSize,&tsz,0)==0) {
                            SFFree(blkBuffer);
                            SFFree(lpBlockTable);
                            SFFree(lpHashTable);
                            SFFree(buffer);
                            CloseHandle(hFile);
                            DeleteFile(lpFileName);
                            return FALSE;

                        }
                        DecryptData((LPBYTE)blkBuffer,blockSize,dwOldCryptKey+k);
                        EncryptData((LPBYTE)blkBuffer,blockSize,dwNewCryptKey+k);
                        SFSetFilePointer(hFile,lpBlockTable[i-nBlkOffset].dwFileOffset+HeaderLength+k,FILE_BEGIN);
                        WriteFile(hFile,blkBuffer,blockSize,&tsz,0);
                    }
                    SFFree(blkBuffer);
                    dwWritten = TRUE;
                }
            }
            if (dwWritten==FALSE) {
                ReadSize = 65536;
                for (j=0;j<mpqOpenArc->lpBlockTable[i].dwCompressedSize;j+=65536) {
                    SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[i].dwFileOffset+j,FILE_BEGIN);
                    SFSetFilePointer(hFile,lpBlockTable[i-nBlkOffset].dwFileOffset+j,FILE_BEGIN);
                    if (j+65536>mpqOpenArc->lpBlockTable[i].dwCompressedSize) ReadSize = mpqOpenArc->lpBlockTable[i].dwCompressedSize-j;
                    if (ReadFile(mpqOpenArc->hFile,buffer,ReadSize,&tsz,0)==0) {
                        SFFree(lpBlockTable);
                        SFFree(lpHashTable);
                        SFFree(buffer);
                        CloseHandle(hFile);
                        DeleteFile(lpFileName);
                        return FALSE;
                    }
                    if (WriteFile(hFile,buffer,ReadSize,&tsz,0)==0) {
                        SFFree(lpBlockTable);
                        SFFree(lpHashTable);
                        SFFree(buffer);
                        CloseHandle(hFile);
                        DeleteFile(lpFileName);
                        return FALSE;
                    }
                }
            }
        }
        else {
            for (j=0;j<mpqOpenArc->MpqHeader.dwHashTableSize;j++) {
                if ((lpHashTable[j].dwBlockTableIndex&0xFFFFFFFE)!=0xFFFFFFFE)
                    if (lpHashTable[j].dwBlockTableIndex>(i-nBlkOffset)) lpHashTable[j].dwBlockTableIndex--;
            }
            nBlkOffset++;
        }
    }
    mpqOpenArc->MpqHeader.dwBlockTableSize -= nBlkOffset;
    mpqOpenArc->MpqHeader.dwHashTableOffset = dwLastOffset;
    dwLastOffset += mpqOpenArc->MpqHeader.dwHashTableSize * sizeof(HASHTABLEENTRY);
    mpqOpenArc->MpqHeader.dwBlockTableOffset = dwLastOffset;
    dwLastOffset += mpqOpenArc->MpqHeader.dwBlockTableSize * sizeof(BLOCKTABLEENTRY);
    mpqOpenArc->MpqHeader.dwMPQSize = dwLastOffset;
    SFFree(mpqOpenArc->lpHashTable);
    mpqOpenArc->lpHashTable = lpHashTable;
    if(mpqOpenArc->lpBlockTable!=0) {
        SFFree(mpqOpenArc->lpBlockTable);
        mpqOpenArc->lpBlockTable = (BLOCKTABLEENTRY *)SFAlloc(mpqOpenArc->MpqHeader.dwBlockTableSize * sizeof(BLOCKTABLEENTRY));
        if (mpqOpenArc->lpBlockTable==0) {
            mpqOpenArc->lpBlockTable = lpBlockTable;
        }
        else {
            memcpy(mpqOpenArc->lpBlockTable,lpBlockTable,mpqOpenArc->MpqHeader.dwBlockTableSize * sizeof(BLOCKTABLEENTRY));
            SFFree(lpBlockTable);
        }
    }
    SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+dwLastOffset,FILE_BEGIN);
    SetEndOfFile(mpqOpenArc->hFile);
    SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart,FILE_BEGIN);
    mpqOpenArc->MpqHeader.dwHeaderSize = sizeof(MPQHEADER);
    WriteFile(mpqOpenArc->hFile,&mpqOpenArc->MpqHeader,sizeof(MPQHEADER),&tsz,0);
    dwLastOffset = sizeof(MPQHEADER);
    ReadSize = 65536;
    for (i=dwLastOffset;i<mpqOpenArc->MpqHeader.dwHashTableOffset;i+=65536) {
        SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+i,FILE_BEGIN);
        SFSetFilePointer(hFile,i,FILE_BEGIN);
        if (i+65536>mpqOpenArc->MpqHeader.dwHashTableOffset) ReadSize = mpqOpenArc->MpqHeader.dwHashTableOffset-i;
        ReadFile(hFile,buffer,ReadSize,&tsz,0);
        WriteFile(mpqOpenArc->hFile,buffer,ReadSize,&tsz,0);
    }
    SFFree(buffer);
    CloseHandle(hFile);
    DeleteFile(lpFileName);
    WriteHashTable(mpqOpenArc);
    WriteBlockTable(mpqOpenArc);
    return TRUE;
}

BOOL SFMPQAPI WINAPI MpqSetFileLocale(MPQHANDLE hMPQ, LPCSTR lpFileName, LCID nOldLocale, LCID nNewLocale)
{
    if (!hMPQ || !lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!(((MPQARCHIVE *)hMPQ)->dwFlags&SFILE_OPEN_ALLOW_WRITE)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    if (nOldLocale==nNewLocale) return FALSE;
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    HASHTABLEENTRY *hashEntry = (HASHTABLEENTRY *)GetHashTableEntry(hMPQ,lpFileName,nOldLocale);
    if (hashEntry==0) return FALSE;
    if (hashEntry->lcLocale!=nOldLocale) return FALSE;
    hashEntry->lcLocale = nNewLocale;

    WriteHashTable(mpqOpenArc);
    LCID dwOldLocale=LocaleID;
    LocaleID=nOldLocale;
    RemoveFromInternalListing(hMPQ,lpFileName);

    LocaleID=nNewLocale;
    AddToInternalListing(hMPQ,lpFileName);
    LocaleID=dwOldLocale;
    return TRUE;
}

DWORD SFMPQAPI WINAPI SFileFindMpqHeader(HANDLE hFile)
{
    if (hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0xFFFFFFFF;
    }
    DWORD FileLen = (DWORD)SFGetFileSize(hFile);
    char pbuf[sizeof(MPQHEADER)];
    DWORD tsz;
    for (DWORD i=0;i<FileLen;i+=512)
    {
        SFSetFilePointer(hFile,i,FILE_BEGIN);
        if (ReadFile(hFile,pbuf,sizeof(MPQHEADER),&tsz,0)==0) return 0xFFFFFFFF;
        if (tsz<sizeof(MPQHEADER)) return 0xFFFFFFFF;
        if (memcmp(pbuf,ID_MPQ,4)==0 || memcmp(pbuf,ID_BN3,4)==0)
        {
            // Storm no longer does this, so mpq api shouldn't either
            /*FileLen -= i;
            if (memcmp(pbuf+8,&FileLen,4)==0)
                return i;
            else
                FileLen += i;*/
            return i;
        }
    }
    return 0xFFFFFFFF;
}

DWORD WINAPI FindMpqHeaderAtLocation(HANDLE hFile, DWORD dwStart, DWORD dwLength)
{
    if (hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0xFFFFFFFF;
    }
    char pbuf[sizeof(MPQHEADER)];
    DWORD tsz;
    for (DWORD i=dwStart;i<dwStart+dwLength;i+=512)
    {
        SFSetFilePointer(hFile,i,FILE_BEGIN);
        if (ReadFile(hFile,pbuf,sizeof(MPQHEADER),&tsz,0)==0) return 0xFFFFFFFF;
        if (i+tsz>dwStart+dwLength) tsz = (dwStart+dwLength)-i;
        if (tsz<sizeof(MPQHEADER)) return 0xFFFFFFFF;
        if (memcmp(pbuf,ID_MPQ,4)==0 || memcmp(pbuf,ID_BN3,4)==0)
        {
            // Storm no longer does this, so mpq api shouldn't either
            /*FileLen -= i;
            if (memcmp(pbuf+8,&FileLen,4)==0)
                return i;
            else
                FileLen += i;*/
            return i;
        }
    }
    return 0xFFFFFFFF;
}

DWORD GetFullPath(LPCSTR lpFileName, char *lpBuffer, DWORD dwBufferLength)
{
    if (!lpFileName) return (DWORD)-1;
    DWORD slen = strlen(lpFileName);
    if (memcmp(lpFileName+1,":\\",2)==0) {
        if (slen+1>dwBufferLength) return slen+1;
        memcpy(lpBuffer,lpFileName,slen+1);
    }
#ifdef _WIN32
    else if (lpFileName[0]=='\\') {
#else
    else if (lpFileName[0]=='/') {
#endif
        if (slen+3>dwBufferLength) return slen+3;
        memcpy(lpBuffer,StormBasePath,2);
        memcpy(lpBuffer+2,lpFileName,slen+1);
    }
    else {
        DWORD sbslen = strlen(StormBasePath);
        if (sbslen+slen+1>dwBufferLength) return sbslen+slen+1;
        memcpy(lpBuffer,StormBasePath,sbslen);
        memcpy(lpBuffer+sbslen,lpFileName,slen);
        lpBuffer[sbslen+slen]=0;
    }
    return 0;
}

MPQHANDLE GetHashTableEntry(MPQHANDLE hMPQ, LPCSTR lpFileName, LCID FileLocale)
{
    if (!hMPQ || !lpFileName) return 0;
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    DWORD dwTablePos = HashString(lpFileName,HASH_POSITION) % mpqOpenArc->MpqHeader.dwHashTableSize;
    DWORD dwNameHashA = HashString(lpFileName,HASH_NAME_A);
    DWORD dwNameHashB = HashString(lpFileName,HASH_NAME_B);
    StartTableSearch:
    DWORD i=dwTablePos;
    do
    {
        if (mpqOpenArc->lpHashTable[i].dwBlockTableIndex==0xFFFFFFFF)
        {
            break;
        }
        else if (mpqOpenArc->lpHashTable[i].dwNameHashA==dwNameHashA && mpqOpenArc->lpHashTable[i].dwNameHashB==dwNameHashB && mpqOpenArc->lpHashTable[i].lcLocale==FileLocale && mpqOpenArc->lpHashTable[i].dwBlockTableIndex!=0xFFFFFFFE)
        {
            return (MPQHANDLE)&mpqOpenArc->lpHashTable[i];
        }
        i = (i + 1) % mpqOpenArc->MpqHeader.dwHashTableSize;
    } while (i!=dwTablePos);
    if (FileLocale!=0) {FileLocale=0;goto StartTableSearch;}
    return 0;
}

MPQHANDLE GetHashTableEntryOfHash(MPQHANDLE hMPQ, DWORD dwTablePos, DWORD dwNameHashA, DWORD dwNameHashB, LCID FileLocale)
{
    if (!hMPQ) return 0;
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    StartTableSearch:
    DWORD i=dwTablePos;
    do
    {
        if (mpqOpenArc->lpHashTable[i].dwBlockTableIndex==0xFFFFFFFF)
        {
            break;
        }
        else if (mpqOpenArc->lpHashTable[i].dwNameHashA==dwNameHashA && mpqOpenArc->lpHashTable[i].dwNameHashB==dwNameHashB && mpqOpenArc->lpHashTable[i].lcLocale==FileLocale && mpqOpenArc->lpHashTable[i].dwBlockTableIndex!=0xFFFFFFFE)
        {
            return (MPQHANDLE)&mpqOpenArc->lpHashTable[i];
        }
        i = (i + 1) % mpqOpenArc->MpqHeader.dwHashTableSize;
    } while (i!=dwTablePos);
    if (FileLocale!=0) {FileLocale=0;goto StartTableSearch;}
    return 0;
}

MPQHANDLE GetFreeHashTableEntry(MPQHANDLE hMPQ, LPCSTR lpFileName, LCID FileLocale, BOOL ReturnExisting)
{
    if (!hMPQ || !lpFileName) return 0;
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    DWORD dwTablePos = HashString(lpFileName,HASH_POSITION) % mpqOpenArc->MpqHeader.dwHashTableSize;
    DWORD dwNameHashA = HashString(lpFileName,HASH_NAME_A);
    DWORD dwNameHashB = HashString(lpFileName,HASH_NAME_B);
    DWORD i=dwTablePos, nFirstFree = 0xFFFFFFFF;
    do
    {
        if ((mpqOpenArc->lpHashTable[i].dwBlockTableIndex&0xFFFFFFFE)==0xFFFFFFFE)
        {
            if (nFirstFree == 0xFFFFFFFF || mpqOpenArc->lpHashTable[i].dwBlockTableIndex == 0xFFFFFFFF)
            {
                if (mpqOpenArc->lpHashTable[i].dwBlockTableIndex == 0xFFFFFFFF)
                {
                    if (nFirstFree == 0xFFFFFFFF)
                        return (MPQHANDLE)&mpqOpenArc->lpHashTable[i];
                    else
                        return (MPQHANDLE)&mpqOpenArc->lpHashTable[nFirstFree];
                }
                else nFirstFree = i;
            }
        }
        else if (mpqOpenArc->lpHashTable[i].dwNameHashA==dwNameHashA && mpqOpenArc->lpHashTable[i].dwNameHashB==dwNameHashB && mpqOpenArc->lpHashTable[i].lcLocale==FileLocale)
        {
            if (ReturnExisting!=FALSE)
                return (MPQHANDLE)&mpqOpenArc->lpHashTable[i];
            else {
                SetLastError(MPQ_ERROR_ALREADY_EXISTS);
                return 0;
            }
        }
        i = (i + 1) % mpqOpenArc->MpqHeader.dwHashTableSize;
    } while (i!=dwTablePos);
    if (nFirstFree != 0xFFFFFFFF)
        return (MPQHANDLE)&mpqOpenArc->lpHashTable[nFirstFree];
    SetLastError(MPQ_ERROR_HASH_TABLE_FULL);
    return 0;
}

BOOL SearchOpenArchives(LPCSTR lpFileName, MPQHANDLE *hMPQ, MPQHANDLE *hFile)
{
    MPQHANDLE hnMPQ,hnFile,hndMPQ = 0,hndFile = 0;

    DWORD dwTablePos = HashString(lpFileName,HASH_POSITION);
    DWORD dwNameHashA = HashString(lpFileName,HASH_NAME_A);
    DWORD dwNameHashB = HashString(lpFileName,HASH_NAME_B);

    for (DWORD i=0;i<dwOpenMpqCount;i++) {
        hnMPQ = (MPQHANDLE)lpOpenMpq[i];
        hnFile = GetHashTableEntryOfHash(hnMPQ,dwTablePos % lpOpenMpq[i]->MpqHeader.dwHashTableSize,dwNameHashA,dwNameHashB,LocaleID);

        if (hnFile!=0) {
            if (((HASHTABLEENTRY *)hnFile)->lcLocale == LocaleID) {
                *hMPQ = hnMPQ;
                *hFile = hnFile;
                return TRUE;
            }
            else if (hndMPQ == 0 || hndFile == 0) {
                hndMPQ = hnMPQ;
                hndFile = hnFile;
            }
        }
    }

    if (hndMPQ != 0 && hndFile != 0) {
        *hMPQ = hndMPQ;
        *hFile = hndFile;
        return TRUE;
    }

    *hMPQ = 0;
    *hFile = 0;
    return FALSE;
}

void SortOpenArchivesByPriority()
{
    MPQARCHIVE *hMPQ1,*hMPQ2;
    for (DWORD i=1;i<dwOpenMpqCount;i++) {
        do {
            hMPQ1 = lpOpenMpq[i-1];
            hMPQ2 = lpOpenMpq[i];
            if (hMPQ2->dwPriority > hMPQ1->dwPriority) {
                lpOpenMpq[i-1] = hMPQ2;
                lpOpenMpq[i] = hMPQ1;
                i--;
            }
        } while (hMPQ2->dwPriority > hMPQ1->dwPriority && i>0);
    }
}

DWORD GetHandleType(MPQHANDLE hFile)
{
    DWORD i;
    for (i=0;i<dwOpenFileCount;i++) {
        if ((MPQHANDLE)lpOpenFile[i]==hFile) return SFILE_TYPE_FILE;
    }
    for (i=0;i<dwOpenMpqCount;i++) {
        if ((MPQHANDLE)lpOpenMpq[i]==hFile) return SFILE_TYPE_MPQ;
    }
    return 0;
}

BOOL AddToInternalListing(MPQHANDLE hMPQ, LPCSTR lpFileName)
{
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    if ((mpqOpenArc->dwFlags & MOAU_MAINTAIN_LISTFILE)==MOAU_MAINTAIN_LISTFILE && _stricmp(lpFileName,INTERNAL_LISTFILE)!=0)
    {
        MPQHANDLE hlFile;char *buffer=0;DWORD fsz,lsz;
        lsz = strlen(lpFileName);
        if (SFileOpenFileEx(hMPQ,INTERNAL_LISTFILE,0,&hlFile)!=0) {
            if (((MPQFILE *)hlFile)->lpHashEntry->lcLocale==LocaleID) {
                fsz = SFileGetFileSize(hlFile,0);
                if (fsz==0) {
                    SFileCloseFile(hlFile);
                    goto AddFileName;
                }
                buffer = (char *)SFAlloc(fsz+lsz+2);
                if (buffer==0) {
                    SFileCloseFile(hlFile);
                    return FALSE;
                }
                if (SFileReadFile(hlFile,buffer,fsz,0,0)==0) {
                    SFFree(buffer);
                    buffer = 0;
                }
                buffer[fsz]=0;
            }
            SFileCloseFile(hlFile);
        }
        AddFileName:
        if (buffer==0) {
            fsz = 0;
            buffer = (char *)SFAlloc(lsz+2);
            buffer[0]=0;
        }
        else {
            char *buffercopy = _strlwr(_strdup(buffer));
            char *lwrFileName = _strlwr(_strdup(lpFileName));
            char *subbuffer=buffer;
            while ((subbuffer=strstr(subbuffer,lpFileName))!=NULL && subbuffer[0]!=0) {
                if (subbuffer==buffer && subbuffer[lsz]==0) {
                    SFFree(lwrFileName);
                    SFFree(buffercopy);
                    SFFree(buffer);
                    return TRUE;
                }
                else if (subbuffer==buffer && (subbuffer[lsz]=='\n' || subbuffer[lsz]=='\r')) {
                    SFFree(lwrFileName);
                    SFFree(buffercopy);
                    SFFree(buffer);
                    return TRUE;
                }
                else if (((subbuffer-1)[0]=='\n' || (subbuffer-1)[0]=='\r') && subbuffer[lsz]==0) {
                    SFFree(lwrFileName);
                    SFFree(buffercopy);
                    SFFree(buffer);
                    return TRUE;
                }
                else if (((subbuffer-1)[0]=='\n' || (subbuffer-1)[0]=='\r') && (subbuffer[lsz]=='\n' || subbuffer[lsz]=='\r')) {
                    SFFree(lwrFileName);
                    SFFree(buffercopy);
                    SFFree(buffer);
                    return TRUE;
                }
                subbuffer++;
            }
            SFFree(lwrFileName);
            SFFree(buffercopy);
        }
        memcpy(buffer+fsz,lpFileName,lsz);
        memcpy(buffer+fsz+lsz,"\r\n",2);
        //LCID dwOldLocale=LocaleID;
        //LocaleID=0;
        MpqAddFileFromBuffer(hMPQ,buffer,fsz+lsz+2,INTERNAL_LISTFILE,MAFA_COMPRESS|MAFA_ENCRYPT|MAFA_MODCRYPTKEY|MAFA_REPLACE_EXISTING);
        //LocaleID=dwOldLocale;
        SFFree(buffer);
    }
    return TRUE;
}

BOOL RemoveFromInternalListing(MPQHANDLE hMPQ, LPCSTR lpFileName)
{
    MPQARCHIVE *mpqOpenArc = (MPQARCHIVE *)hMPQ;
    if ((mpqOpenArc->dwFlags & MOAU_MAINTAIN_LISTFILE)==MOAU_MAINTAIN_LISTFILE && _stricmp(lpFileName,INTERNAL_LISTFILE)!=0)
    {
        MPQHANDLE hlFile;char *buffer=0;DWORD fsz,lsz;
        lsz = strlen(lpFileName);
        if (SFileOpenFileEx(hMPQ,INTERNAL_LISTFILE,0,&hlFile)!=0) {
            if (((MPQFILE *)hlFile)->lpHashEntry->lcLocale==LocaleID) {
                fsz = SFileGetFileSize(hlFile,0);
                if (fsz==0) {
                    SFileCloseFile(hlFile);

                    return FALSE;
                }

                buffer = (char *)SFAlloc(fsz+1);
                if (buffer==0) {
                    SFileCloseFile(hlFile);
                    return FALSE;
                }
                buffer[fsz] = 0;
                if (SFileReadFile(hlFile,buffer,fsz,0,0)==0) {
                    SFFree(buffer);
                    buffer = 0;
                }
            }
            SFileCloseFile(hlFile);
        }
        if (buffer==0) {
            return FALSE;
        }
        else {
            buffer[fsz]=0;
            char *buffercopy = _strlwr(_strdup(buffer));
            char *lwrFileName = _strlwr(_strdup(lpFileName));
            char *subbuffer=buffer;

            while ((subbuffer=strstr(subbuffer,lpFileName))!=NULL && subbuffer[0]!=0) {
                if (subbuffer==buffer && subbuffer[lsz]==0) {
                    SFFree(lwrFileName);
                    SFFree(buffercopy);
                    //LCID dwOldLocale=LocaleID;
                    //LocaleID=0;
                    MpqAddFileFromBuffer(hMPQ,"\x00",0,INTERNAL_LISTFILE,MAFA_COMPRESS|MAFA_ENCRYPT|MAFA_MODCRYPTKEY|MAFA_REPLACE_EXISTING);
                    //LocaleID=dwOldLocale;
                    SFFree(buffer);
                    return TRUE;
                }
                else if (subbuffer==buffer && (subbuffer[lsz]=='\n' || subbuffer[lsz]=='\r')) {
                    SFFree(lwrFileName);
                    SFFree(buffercopy);
                    if (subbuffer[lsz+1]=='\n' || subbuffer[lsz+1]=='\r') lsz++;
                    memcpy(subbuffer,subbuffer+lsz+1,strlen(subbuffer+lsz+1));
                    //LCID dwOldLocale=LocaleID;
                    //LocaleID=0;
                    MpqAddFileFromBuffer(hMPQ,buffer,fsz-lsz-1,INTERNAL_LISTFILE,MAFA_COMPRESS|MAFA_ENCRYPT|MAFA_MODCRYPTKEY|MAFA_REPLACE_EXISTING);
                    //LocaleID=dwOldLocale;
                    SFFree(buffer);
                    return TRUE;
                }
                else if (((subbuffer-1)[0]=='\n' || (subbuffer-1)[0]=='\r') && subbuffer[lsz]==0) {
                    SFFree(lwrFileName);
                    SFFree(buffercopy);
                    //LCID dwOldLocale=LocaleID;
                    //LocaleID=0;
                    MpqAddFileFromBuffer(hMPQ,buffer,fsz-lsz,INTERNAL_LISTFILE,MAFA_COMPRESS|MAFA_ENCRYPT|MAFA_MODCRYPTKEY|MAFA_REPLACE_EXISTING);
                    //LocaleID=dwOldLocale;
                    SFFree(buffer);
                    return TRUE;
                }
                else if (((subbuffer-1)[0]=='\n' || (subbuffer-1)[0]=='\r') && (subbuffer[lsz]=='\n' || subbuffer[lsz]=='\r')) {
                    SFFree(lwrFileName);
                    SFFree(buffercopy);
                    if ((subbuffer-2)[0]=='\n' || (subbuffer-2)[0]=='\r') {subbuffer--;lsz++;}
                    memcpy(subbuffer-1,subbuffer+lsz,strlen(subbuffer+lsz));
                    //LCID dwOldLocale=LocaleID;
                    //LocaleID=0;
                    MpqAddFileFromBuffer(hMPQ,buffer,fsz-lsz-1,INTERNAL_LISTFILE,MAFA_COMPRESS|MAFA_ENCRYPT|MAFA_MODCRYPTKEY|MAFA_REPLACE_EXISTING);
                    //LocaleID=dwOldLocale;
                    SFFree(buffer);
                    return TRUE;
                }
                subbuffer++;
            }
            SFFree(lwrFileName);
            SFFree(buffercopy);
        }
        SFFree(buffer);
    }
    return TRUE;
}

DWORD DetectFileSeedEx(MPQARCHIVE * mpqOpenArc, HASHTABLEENTRY * lpHashEntry, LPSTR * lplpFileName)
{
    if (mpqOpenArc==0 || lpHashEntry==0) return 0;
    DWORD dwCryptKey=0;
    LPCSTR lpFileName = *lplpFileName;
    DWORD dwBlockIndex = lpHashEntry->dwBlockTableIndex;
    if (lpFileName)
    {
        dwCryptKey = HashString(lpFileName,HASH_KEY);
        if (mpqOpenArc->lpBlockTable[dwBlockIndex].dwFlags&MAFA_MODCRYPTKEY)
            dwCryptKey = (dwCryptKey + mpqOpenArc->lpBlockTable[dwBlockIndex].dwFileOffset) ^ mpqOpenArc->lpBlockTable[dwBlockIndex].dwFullSize;
    }
    else
    {
        DWORD dwNameHashA = HashString(INTERNAL_LISTFILE,HASH_NAME_A);
        DWORD dwNameHashB = HashString(INTERNAL_LISTFILE,HASH_NAME_B);
        if (lpHashEntry->dwNameHashA==dwNameHashA && lpHashEntry->dwNameHashB==dwNameHashB) {
            dwCryptKey = HashString(INTERNAL_LISTFILE,HASH_KEY);
            if (mpqOpenArc->lpBlockTable[dwBlockIndex].dwFlags&MAFA_MODCRYPTKEY)
                dwCryptKey = (dwCryptKey + mpqOpenArc->lpBlockTable[dwBlockIndex].dwFileOffset) ^ mpqOpenArc->lpBlockTable[dwBlockIndex].dwFullSize;
            char* s = (char *)SFAlloc(strlen(INTERNAL_LISTFILE)+1);
            if (s)
                strcpy(s,INTERNAL_LISTFILE);
            *lplpFileName = s;
        }
        else {
            HANDLE hlFile,hMPQ=(HANDLE)mpqOpenArc;
            DWORD fsz;
            char *listbuffer = 0;
            LCID lcOldLocale = LocaleID;
            for (DWORD lcn=0;lcn<nLocales;lcn++) {
                LocaleID = availLocales[lcn];
                if (SFileOpenFileEx(hMPQ,INTERNAL_LISTFILE,0,&hlFile)!=0) {
                    if (((MPQFILE *)hlFile)->lpHashEntry->lcLocale==0 && LocaleID!=0) {
                        SFileCloseFile(hlFile);
                        continue;
                    }
                    fsz = SFileGetFileSize(hlFile,0);
                    if (fsz>0) {
                        listbuffer = (char *)SFAlloc(fsz+1);
                        if (listbuffer==0) {
                            SFileCloseFile(hlFile);
                            continue;
                        }
                        if (SFileReadFile(hlFile,listbuffer,fsz,0,0)==0) {
                            SFFree(listbuffer);
                            listbuffer = 0;
                        }
                    }
                    SFileCloseFile(hlFile);
                    if (listbuffer!=0) {
                        char *listline = 0;
                        for (listline=listbuffer;listline!=0;listline=nextline(listline)) {
                            if (listline[0]==0) break;
                            DWORD lnlen=strlnlen(listline);
                            char prevchar=listline[lnlen];
                            listline[lnlen]=0;
                            dwNameHashA = HashString(listline,HASH_NAME_A);
                            dwNameHashB = HashString(listline,HASH_NAME_B);
                            if (lpHashEntry->dwNameHashA==dwNameHashA && lpHashEntry->dwNameHashB==dwNameHashB) {
                                dwCryptKey = HashString(listline,HASH_KEY);
                                if (mpqOpenArc->lpBlockTable[dwBlockIndex].dwFlags&MAFA_MODCRYPTKEY)
                                    dwCryptKey = (dwCryptKey + mpqOpenArc->lpBlockTable[dwBlockIndex].dwFileOffset) ^ mpqOpenArc->lpBlockTable[dwBlockIndex].dwFullSize;
                                char* s = (char *)SFAlloc(strlen(listline)+1);
                                if (s)
                                    strcpy(s,listline);
                                *lplpFileName = s;
                                break;
                            }
                            listline[lnlen]=prevchar;
                        }
                        if (listline!=0) {
                            if (listline[0]!=0) {
                                SFFree(listbuffer);
                                break;
                            }
                        }
                        SFFree(listbuffer);
                    }
                }
            }
            LocaleID = lcOldLocale;
        }
        if (dwCryptKey==0 && (mpqOpenArc->lpBlockTable[dwBlockIndex].dwFlags&MAFA_COMPRESS || mpqOpenArc->lpBlockTable[dwBlockIndex].dwFlags&MAFA_COMPRESS2))
        {
            DWORD HeaderLength=0,tsz;
            if (memcmp(&mpqOpenArc->MpqHeader.dwMPQID,ID_BN3,4)==0)
            {
                SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[dwBlockIndex].dwFileOffset,FILE_BEGIN);
                ReadFile(mpqOpenArc->hFile,&HeaderLength,4,&tsz,0);
            }
            DWORD blockSize = 512 << mpqOpenArc->MpqHeader.wBlockSize;
            DWORD TotalBlocks = mpqOpenArc->lpBlockTable[dwBlockIndex].dwFullSize / blockSize;
            if(mpqOpenArc->lpBlockTable[dwBlockIndex].dwFullSize % blockSize)
                TotalBlocks++;
            DWORD *dwBlockPtrTable = (DWORD *)SFAlloc((TotalBlocks+1)*4);
            if (dwBlockPtrTable==0)
                return 0;
            SFSetFilePointer(mpqOpenArc->hFile,mpqOpenArc->dwMPQStart+mpqOpenArc->lpBlockTable[dwBlockIndex].dwFileOffset+HeaderLength,FILE_BEGIN);
            ReadFile(mpqOpenArc->hFile,dwBlockPtrTable,(TotalBlocks+1)*4,&tsz,0);
            dwCryptKey = DetectFileSeed(dwBlockPtrTable,(TotalBlocks+1)*4,blockSize);

            SFFree(dwBlockPtrTable);
        }
    }
    return dwCryptKey;
}

long SFMPQAPI __inline SFMpqCompareVersion()
{
    SFMPQVERSION ExeVersion = SFMPQ_CURRENT_VERSION;
    SFMPQVERSION DllVersion = SFMpqGetVersion();
    if (DllVersion.Major>ExeVersion.Major) return 1;
    else if (DllVersion.Major<ExeVersion.Major) return -1;
    if (DllVersion.Minor>ExeVersion.Minor) return 1;
    else if (DllVersion.Minor<ExeVersion.Minor) return -1;
    if (DllVersion.Revision>ExeVersion.Revision) return 1;
    else if (DllVersion.Revision<ExeVersion.Revision) return -1;
    if (DllVersion.Subrevision>ExeVersion.Subrevision) return 1;
    else if (DllVersion.Subrevision<ExeVersion.Subrevision) return -1;
    return 0;
}

void LoadStorm()
{
#ifdef _WIN32
    if (hStorm!=0) return;
    hStorm = LoadLibrary(Storm_dll);
    /*if (hStorm==0) {
        HKEY hKey;
        LPSTR lpDllPath;
        DWORD dwStrLen;
        if (RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Blizzard Entertainment\\Warcraft III",0,KEY_QUERY_VALUE,&hKey)!=ERROR_SUCCESS)
            if (RegOpenKeyEx(HKEY_USERS,".Default\\Software\\Blizzard Entertainment\\Warcraft III",0,KEY_QUERY_VALUE,&hKey)!=ERROR_SUCCESS)
                if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\Blizzard Entertainment\\Warcraft III",0,KEY_QUERY_VALUE,&hKey)!=ERROR_SUCCESS)
                    return;
        RegQueryValueEx(hKey,"InstallPath",0,0,0,&dwStrLen);
        lpDllPath = (LPSTR)SFAlloc(dwStrLen+11);
        if (lpDllPath) {
            memset(lpDllPath,0,dwStrLen+11);
            RegQueryValueEx(hKey,"InstallPath",0,0,(LPBYTE)lpDllPath,&dwStrLen);
            LPSTR lpLastBSlash = strrchr(lpDllPath,'\\');
            if (lpLastBSlash) {
                if (lpLastBSlash[1]!=0) lpLastBSlash[strlen(lpLastBSlash)]='\\';
                strcat(lpLastBSlash,Storm_dll);
                hStorm = LoadLibrary(lpDllPath);
            }
            SFFree(lpDllPath);
        }
        RegCloseKey(hKey);
    }*/
    if (hStorm==0) return;

    unsigned int wSCCOrdinal=0xAC30;
    wSCCOrdinal>>=4;
    wSCCOrdinal/=5;
    unsigned int wSCDcOrdinal=0xAC80;
    wSCDcOrdinal>>=4;
    wSCDcOrdinal/=5;

    stormSCompCompress = (funcSCompCompress)GetProcAddress(hStorm,(LPCSTR)wSCCOrdinal);
    stormSCompDecompress = (funcSCompDecompress)GetProcAddress(hStorm,(LPCSTR)wSCDcOrdinal);
#endif
}

void FreeStorm()
{
#ifdef _WIN32
    if (hStorm==0) return;
    FreeLibrary(hStorm);
    hStorm = 0;

    stormSCompCompress = 0;
    stormSCompDecompress = 0;
#endif
}

