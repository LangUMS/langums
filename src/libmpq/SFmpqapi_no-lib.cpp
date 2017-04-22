/* License information for this code is in license.txt */

// Code for loading SFmpqapi at run-time

// Comment out the next line to load SFmpqapi at the
// start of your program, rather that when it is first
// used.
#define SFMPQAPI_DELAY_LOAD

#include "SFmpqapi_no-lib.h"

struct SFMPQAPI_DELAY_LOADER {
#ifndef SFMPQAPI_DELAY_LOAD
	SFMPQAPI_DELAY_LOADER();
#endif
	~SFMPQAPI_DELAY_LOADER();
} SFMpqApi_Delay_Loader;

HINSTANCE hSFMpq = 0;

void LoadSFMpqDll();

void LoadSFMpqDll()
{
	if (!hSFMpq) hSFMpq = LoadLibrary("SFmpq.dll");
}

BOOL WINAPI MpqInitialize_stub()
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqInitialize = GetProcAddress(hSFMpq,"MpqInitialize");
		if (MpqInitialize) return MpqInitialize();
	}
	return FALSE;
}

LPCSTR WINAPI MpqGetVersionString_stub()
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqGetVersionString = GetProcAddress(hSFMpq,"MpqGetVersionString");
		if (MpqGetVersionString) return MpqGetVersionString();
	}
	return 0;
}

float WINAPI MpqGetVersion_stub()
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqGetVersion = GetProcAddress(hSFMpq,"MpqGetVersion");
		if (MpqGetVersion) return MpqGetVersion();
	}
	return 0;
}

void WINAPI SFMpqDestroy_stub()
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFMpqDestroy = GetProcAddress(hSFMpq,"SFMpqDestroy");
		if (SFMpqDestroy) SFMpqDestroy();
	}
}

void WINAPI AboutSFMpq_stub()
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&AboutSFMpq = GetProcAddress(hSFMpq,"AboutSFMpq");
		if (AboutSFMpq) AboutSFMpq();
	}
}

LPCSTR WINAPI SFMpqGetVersionString_stub()
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFMpqGetVersionString = GetProcAddress(hSFMpq,"SFMpqGetVersionString");
		if (SFMpqGetVersionString) return SFMpqGetVersionString();
	}
	return 0;
}

DWORD WINAPI SFMpqGetVersionString2_stub(LPCSTR lpBuffer, DWORD dwBufferLength)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFMpqGetVersionString2 = GetProcAddress(hSFMpq,"SFMpqGetVersionString2");
		if (SFMpqGetVersionString2) return SFMpqGetVersionString2(lpBuffer,dwBufferLength);
	}
	return 0;
}

SFMPQVERSION WINAPI SFMpqGetVersion_stub()
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFMpqGetVersion = GetProcAddress(hSFMpq,"SFMpqGetVersion");
		if (SFMpqGetVersion) return SFMpqGetVersion();
	}
	SFMPQVERSION NoVersionData = {0,0,0,0};
	return NoVersionData;
}

BOOL WINAPI SFileOpenArchive_stub(LPCSTR lpFileName, DWORD dwPriority, DWORD dwFlags, MPQHANDLE *hMPQ)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileOpenArchive = GetProcAddress(hSFMpq,"SFileOpenArchive");
		if (SFileOpenArchive) return SFileOpenArchive(lpFileName,dwPriority,dwFlags,hMPQ);
	}
	return FALSE;
}

BOOL WINAPI SFileCloseArchive_stub(MPQHANDLE hMPQ)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileCloseArchive = GetProcAddress(hSFMpq,"SFileCloseArchive");
		if (SFileCloseArchive) return SFileCloseArchive(hMPQ);
	}
	return FALSE;
}

BOOL WINAPI SFileOpenFileAsArchive_stub(MPQHANDLE hSourceMPQ, LPCSTR lpFileName, DWORD dwPriority, DWORD dwFlags, MPQHANDLE *hMPQ)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileOpenFileAsArchive = GetProcAddress(hSFMpq,"SFileOpenFileAsArchive");
		if (SFileOpenFileAsArchive) return SFileOpenFileAsArchive(hSourceMPQ,lpFileName,dwPriority,dwFlags,hMPQ);
	}
	return FALSE;
}

BOOL WINAPI SFileGetArchiveName_stub(MPQHANDLE hMPQ, LPCSTR lpBuffer, DWORD dwBufferLength)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileGetArchiveName = GetProcAddress(hSFMpq,"SFileGetArchiveName");
		if (SFileGetArchiveName) return SFileGetArchiveName(hMPQ,lpBuffer,dwBufferLength);
	}
	return FALSE;
}

BOOL WINAPI SFileOpenFile_stub(LPCSTR lpFileName, MPQHANDLE *hFile)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileOpenFile = GetProcAddress(hSFMpq,"SFileOpenFile");
		if (SFileOpenFile) return SFileOpenFile(lpFileName,hFile);
	}
	return FALSE;
}

BOOL WINAPI SFileOpenFileEx_stub(MPQHANDLE hMPQ, LPCSTR lpFileName, DWORD dwSearchScope, MPQHANDLE *hFile)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileOpenFileEx = GetProcAddress(hSFMpq,"SFileOpenFileEx");
		if (SFileOpenFileEx) return SFileOpenFileEx(hMPQ,lpFileName,dwSearchScope,hFile);
	}
	return FALSE;
}

BOOL WINAPI SFileCloseFile_stub(MPQHANDLE hFile)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileCloseFile = GetProcAddress(hSFMpq,"SFileCloseFile");
		if (SFileCloseFile) return SFileCloseFile(hFile);
	}
	return FALSE;
}

DWORD WINAPI SFileGetFileSize_stub(MPQHANDLE hFile, LPDWORD lpFileSizeHigh)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileGetFileSize = GetProcAddress(hSFMpq,"SFileGetFileSize");
		if (SFileGetFileSize) return SFileGetFileSize(hFile,lpFileSizeHigh);
	}
	return (DWORD)-1;
}

BOOL WINAPI SFileGetFileArchive_stub(MPQHANDLE hFile, MPQHANDLE *hMPQ)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileGetFileArchive = GetProcAddress(hSFMpq,"SFileGetFileArchive");
		if (SFileGetFileArchive) return SFileGetFileArchive(hFile,hMPQ);
	}
	return FALSE;
}

BOOL WINAPI SFileGetFileName_stub(MPQHANDLE hFile, LPCSTR lpBuffer, DWORD dwBufferLength)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileGetFileName = GetProcAddress(hSFMpq,"SFileGetFileName");
		if (SFileGetFileName) return SFileGetFileName(hFile,lpBuffer,dwBufferLength);
	}
	return FALSE;
}

DWORD WINAPI SFileSetFilePointer_stub(MPQHANDLE hFile, long lDistanceToMove, PLONG lplDistanceToMoveHigh, DWORD dwMoveMethod)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileSetFilePointer = GetProcAddress(hSFMpq,"SFileSetFilePointer");
		if (SFileSetFilePointer) return SFileSetFilePointer(hFile,lDistanceToMove,lplDistanceToMoveHigh,dwMoveMethod);
	}
	return (DWORD)-1;
}

BOOL WINAPI SFileReadFile_stub(MPQHANDLE hFile,LPVOID lpBuffer,DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead,LPOVERLAPPED lpOverlapped)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileReadFile = GetProcAddress(hSFMpq,"SFileReadFile");
		if (SFileReadFile) return SFileReadFile(hFile,lpBuffer,nNumberOfBytesToRead,lpNumberOfBytesRead,lpOverlapped);
	}
	return FALSE;
}

LCID WINAPI SFileSetLocale_stub(LCID nNewLocale)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileSetLocale = GetProcAddress(hSFMpq,"SFileSetLocale");
		if (SFileSetLocale) return SFileSetLocale(nNewLocale);
	}
	return 0;
}

BOOL WINAPI SFileGetBasePath_stub(LPCSTR lpBuffer, DWORD dwBufferLength)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileGetBasePath = GetProcAddress(hSFMpq,"SFileGetBasePath");
		if (SFileGetBasePath) return SFileGetBasePath(lpBuffer,dwBufferLength);
	}
	return FALSE;
}

BOOL WINAPI SFileSetBasePath_stub(LPCSTR lpNewBasePath)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileSetBasePath = GetProcAddress(hSFMpq,"SFileSetBasePath");
		if (SFileSetBasePath) return SFileSetBasePath(lpNewBasePath);
	}
	return FALSE;
}

DWORD WINAPI SFileGetFileInfo_stub(MPQHANDLE hFile, DWORD dwInfoType)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileGetFileInfo = GetProcAddress(hSFMpq,"SFileGetFileInfo");
		if (SFileGetFileInfo) return SFileGetFileInfo(hFile,dwInfoType);
	}
	return (DWORD)-1;
}

BOOL WINAPI SFileSetArchivePriority_stub(MPQHANDLE hMPQ, DWORD dwPriority)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileSetArchivePriority = GetProcAddress(hSFMpq,"SFileSetArchivePriority");
		if (SFileSetArchivePriority) return SFileSetArchivePriority(hMPQ,dwPriority);
	}
	return FALSE;
}

DWORD WINAPI SFileFindMpqHeader_stub(HANDLE hFile)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileFindMpqHeader = GetProcAddress(hSFMpq,"SFileFindMpqHeader");
		if (SFileFindMpqHeader) return SFileFindMpqHeader(hFile);
	}
	return (DWORD)-1;
}

BOOL WINAPI SFileListFiles_stub(MPQHANDLE hMPQ, LPCSTR lpFileLists, FILELISTENTRY *lpListBuffer, DWORD dwFlags)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileListFiles = GetProcAddress(hSFMpq,"SFileListFiles");
		if (SFileListFiles) return SFileListFiles(hMPQ,lpFileLists,lpListBuffer,dwFlags);
	}
	return FALSE;
}

MPQHANDLE WINAPI MpqOpenArchiveForUpdate_stub(LPCSTR lpFileName, DWORD dwFlags, DWORD dwMaximumFilesInArchive)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqOpenArchiveForUpdate = GetProcAddress(hSFMpq,"MpqOpenArchiveForUpdate");
		if (MpqOpenArchiveForUpdate) return MpqOpenArchiveForUpdate(lpFileName,dwFlags,dwMaximumFilesInArchive);
	}
	return INVALID_HANDLE_VALUE;
}

DWORD WINAPI MpqCloseUpdatedArchive_stub(MPQHANDLE hMPQ, DWORD dwUnknown2)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqCloseUpdatedArchive = GetProcAddress(hSFMpq,"MpqCloseUpdatedArchive");
		if (MpqCloseUpdatedArchive) return MpqCloseUpdatedArchive(hMPQ,dwUnknown2);
	}
	return 0;
}

BOOL WINAPI MpqAddFileToArchive_stub(MPQHANDLE hMPQ, LPCSTR lpSourceFileName, LPCSTR lpDestFileName, DWORD dwFlags)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqAddFileToArchive = GetProcAddress(hSFMpq,"MpqAddFileToArchive");
		if (MpqAddFileToArchive) return MpqAddFileToArchive(hMPQ,lpSourceFileName,lpDestFileName,dwFlags);
	}
	return FALSE;
}

BOOL WINAPI MpqAddWaveToArchive_stub(MPQHANDLE hMPQ, LPCSTR lpSourceFileName, LPCSTR lpDestFileName, DWORD dwFlags, DWORD dwQuality)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqAddWaveToArchive = GetProcAddress(hSFMpq,"MpqAddWaveToArchive");
		if (MpqAddWaveToArchive) return MpqAddWaveToArchive(hMPQ,lpSourceFileName,lpDestFileName,dwFlags,dwQuality);
	}
	return FALSE;
}

BOOL WINAPI MpqRenameFile_stub(MPQHANDLE hMPQ, LPCSTR lpcOldFileName, LPCSTR lpcNewFileName)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqRenameFile = GetProcAddress(hSFMpq,"MpqRenameFile");
		if (MpqRenameFile) return MpqRenameFile(hMPQ,lpcOldFileName,lpcNewFileName);
	}
	return FALSE;
}

BOOL WINAPI MpqDeleteFile_stub(MPQHANDLE hMPQ, LPCSTR lpFileName)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqDeleteFile = GetProcAddress(hSFMpq,"MpqDeleteFile");
		if (MpqDeleteFile) return MpqDeleteFile(hMPQ,lpFileName);
	}
	return FALSE;
}

BOOL WINAPI MpqCompactArchive_stub(MPQHANDLE hMPQ)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqCompactArchive = GetProcAddress(hSFMpq,"MpqCompactArchive");
		if (MpqCompactArchive) return MpqCompactArchive(hMPQ);
	}
	return FALSE;
}

MPQHANDLE WINAPI MpqOpenArchiveForUpdateEx_stub(LPCSTR lpFileName, DWORD dwFlags, DWORD dwMaximumFilesInArchive, DWORD dwBlockSize)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqOpenArchiveForUpdateEx = GetProcAddress(hSFMpq,"MpqOpenArchiveForUpdateEx");
		if (MpqOpenArchiveForUpdateEx) return MpqOpenArchiveForUpdateEx(lpFileName,dwFlags,dwMaximumFilesInArchive,dwBlockSize);
	}
	return INVALID_HANDLE_VALUE;
}

BOOL WINAPI MpqAddFileToArchiveEx_stub(MPQHANDLE hMPQ, LPCSTR lpSourceFileName, LPCSTR lpDestFileName, DWORD dwFlags, DWORD dwCompressionType, DWORD dwCompressLevel)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqAddFileToArchiveEx = GetProcAddress(hSFMpq,"MpqAddFileToArchiveEx");
		if (MpqAddFileToArchiveEx) return MpqAddFileToArchiveEx(hMPQ,lpSourceFileName,lpDestFileName,dwFlags,dwCompressionType,dwCompressLevel);
	}
	return FALSE;
}

BOOL WINAPI MpqAddFileFromBufferEx_stub(MPQHANDLE hMPQ, LPVOID lpBuffer, DWORD dwLength, LPCSTR lpFileName, DWORD dwFlags, DWORD dwCompressionType, DWORD dwCompressLevel)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqAddFileFromBufferEx = GetProcAddress(hSFMpq,"MpqAddFileFromBufferEx");
		if (MpqAddFileFromBufferEx) return MpqAddFileFromBufferEx(hMPQ,lpBuffer,dwLength,lpFileName,dwFlags,dwCompressionType,dwCompressLevel);
	}
	return FALSE;
}

BOOL WINAPI MpqAddFileFromBuffer_stub(MPQHANDLE hMPQ, LPVOID lpBuffer, DWORD dwLength, LPCSTR lpFileName, DWORD dwFlags)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqAddFileFromBuffer = GetProcAddress(hSFMpq,"MpqAddFileFromBuffer");
		if (MpqAddFileFromBuffer) return MpqAddFileFromBuffer(hMPQ,lpBuffer,dwLength,lpFileName,dwFlags);
	}
	return FALSE;
}

BOOL WINAPI MpqAddWaveFromBuffer_stub(MPQHANDLE hMPQ, LPVOID lpBuffer, DWORD dwLength, LPCSTR lpFileName, DWORD dwFlags, DWORD dwQuality)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqAddWaveFromBuffer = GetProcAddress(hSFMpq,"MpqAddWaveFromBuffer");
		if (MpqAddWaveFromBuffer) return MpqAddWaveFromBuffer(hMPQ,lpBuffer,dwLength,lpFileName,dwFlags,dwQuality);
	}
	return FALSE;
}

BOOL WINAPI MpqRenameAndSetFileLocale_stub(MPQHANDLE hMPQ, LPCSTR lpcOldFileName, LPCSTR lpcNewFileName, LCID nOldLocale, LCID nNewLocale)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqRenameAndSetFileLocale = GetProcAddress(hSFMpq,"MpqRenameAndSetFileLocale");
		if (MpqRenameAndSetFileLocale) return MpqRenameAndSetFileLocale(hMPQ,lpcOldFileName,lpcNewFileName,nOldLocale,nNewLocale);
	}
	return FALSE;
}

BOOL WINAPI MpqDeleteFileWithLocale_stub(MPQHANDLE hMPQ, LPCSTR lpFileName, LCID nLocale)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqDeleteFileWithLocale = GetProcAddress(hSFMpq,"MpqDeleteFileWithLocale");
		if (MpqDeleteFileWithLocale) return MpqDeleteFileWithLocale(hMPQ,lpFileName,nLocale);
	}
	return FALSE;
}

BOOL WINAPI MpqSetFileLocale_stub(MPQHANDLE hMPQ, LPCSTR lpFileName, LCID nOldLocale, LCID nNewLocale)
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&MpqSetFileLocale = GetProcAddress(hSFMpq,"MpqSetFileLocale");
		if (MpqSetFileLocale) return MpqSetFileLocale(hMPQ,lpFileName,nOldLocale,nNewLocale);
	}
	return FALSE;
}

BOOL WINAPI SFileDestroy_stub()
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&SFileDestroy = GetProcAddress(hSFMpq,"SFileDestroy");
		if (SFileDestroy) return SFileDestroy();
	}
	return FALSE;
}

void WINAPI StormDestroy_stub()
{
	LoadSFMpqDll();
	if (hSFMpq) {
		*(FARPROC *)&StormDestroy = GetProcAddress(hSFMpq,"StormDestroy");
		if (StormDestroy) StormDestroy();
	}
}

BOOL   (WINAPI* MpqInitialize)() = MpqInitialize_stub;

LPCSTR (WINAPI* MpqGetVersionString)() = MpqGetVersionString_stub;
float  (WINAPI* MpqGetVersion)() = MpqGetVersion_stub;

void (WINAPI* SFMpqDestroy)() = SFMpqDestroy_stub;

void (WINAPI* AboutSFMpq)() = AboutSFMpq_stub;

LPCSTR (WINAPI* SFMpqGetVersionString)() = SFMpqGetVersionString_stub;
DWORD  (WINAPI* SFMpqGetVersionString2)(LPCSTR lpBuffer, DWORD dwBufferLength) = SFMpqGetVersionString2_stub;
SFMPQVERSION (WINAPI* SFMpqGetVersion)() = SFMpqGetVersion_stub;

BOOL      (WINAPI* SFileOpenArchive)(LPCSTR lpFileName, DWORD dwPriority, DWORD dwFlags, MPQHANDLE *hMPQ) = SFileOpenArchive_stub;
BOOL      (WINAPI* SFileCloseArchive)(MPQHANDLE hMPQ) = SFileCloseArchive_stub;
BOOL      (WINAPI* SFileOpenFileAsArchive)(MPQHANDLE hSourceMPQ, LPCSTR lpFileName, DWORD dwPriority, DWORD dwFlags, MPQHANDLE *hMPQ) = SFileOpenFileAsArchive_stub;
BOOL      (WINAPI* SFileGetArchiveName)(MPQHANDLE hMPQ, LPCSTR lpBuffer, DWORD dwBufferLength) = SFileGetArchiveName_stub;
BOOL      (WINAPI* SFileOpenFile)(LPCSTR lpFileName, MPQHANDLE *hFile) = SFileOpenFile_stub;
BOOL      (WINAPI* SFileOpenFileEx)(MPQHANDLE hMPQ, LPCSTR lpFileName, DWORD dwSearchScope, MPQHANDLE *hFile) = SFileOpenFileEx_stub;
BOOL      (WINAPI* SFileCloseFile)(MPQHANDLE hFile) = SFileCloseFile_stub;
DWORD     (WINAPI* SFileGetFileSize)(MPQHANDLE hFile, LPDWORD lpFileSizeHigh) = SFileGetFileSize_stub;
BOOL      (WINAPI* SFileGetFileArchive)(MPQHANDLE hFile, MPQHANDLE *hMPQ) = SFileGetFileArchive_stub;
BOOL      (WINAPI* SFileGetFileName)(MPQHANDLE hFile, LPCSTR lpBuffer, DWORD dwBufferLength) = SFileGetFileName_stub;
DWORD     (WINAPI* SFileSetFilePointer)(MPQHANDLE hFile, long lDistanceToMove, PLONG lplDistanceToMoveHigh, DWORD dwMoveMethod) = SFileSetFilePointer_stub;
BOOL      (WINAPI* SFileReadFile)(MPQHANDLE hFile,LPVOID lpBuffer,DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead,LPOVERLAPPED lpOverlapped) = SFileReadFile_stub;
LCID      (WINAPI* SFileSetLocale)(LCID nNewLocale) = SFileSetLocale_stub;
BOOL      (WINAPI* SFileGetBasePath)(LPCSTR lpBuffer, DWORD dwBufferLength) = SFileGetBasePath_stub;
BOOL      (WINAPI* SFileSetBasePath)(LPCSTR lpNewBasePath) = SFileSetBasePath_stub;

DWORD     (WINAPI* SFileGetFileInfo)(MPQHANDLE hFile, DWORD dwInfoType) = SFileGetFileInfo_stub;
BOOL      (WINAPI* SFileSetArchivePriority)(MPQHANDLE hMPQ, DWORD dwPriority) = SFileSetArchivePriority_stub;
DWORD     (WINAPI* SFileFindMpqHeader)(HANDLE hFile) = SFileFindMpqHeader_stub;
BOOL      (WINAPI* SFileListFiles)(MPQHANDLE hMPQ, LPCSTR lpFileLists, FILELISTENTRY *lpListBuffer, DWORD dwFlags) = SFileListFiles_stub;

MPQHANDLE (WINAPI* MpqOpenArchiveForUpdate)(LPCSTR lpFileName, DWORD dwFlags, DWORD dwMaximumFilesInArchive) = MpqOpenArchiveForUpdate_stub;
DWORD     (WINAPI* MpqCloseUpdatedArchive)(MPQHANDLE hMPQ, DWORD dwUnknown2) = MpqCloseUpdatedArchive_stub;
BOOL      (WINAPI* MpqAddFileToArchive)(MPQHANDLE hMPQ, LPCSTR lpSourceFileName, LPCSTR lpDestFileName, DWORD dwFlags) = MpqAddFileToArchive_stub;
BOOL      (WINAPI* MpqAddWaveToArchive)(MPQHANDLE hMPQ, LPCSTR lpSourceFileName, LPCSTR lpDestFileName, DWORD dwFlags, DWORD dwQuality) = MpqAddWaveToArchive_stub;
BOOL      (WINAPI* MpqRenameFile)(MPQHANDLE hMPQ, LPCSTR lpcOldFileName, LPCSTR lpcNewFileName) = MpqRenameFile_stub;
BOOL      (WINAPI* MpqDeleteFile)(MPQHANDLE hMPQ, LPCSTR lpFileName) = MpqDeleteFile_stub;
BOOL      (WINAPI* MpqCompactArchive)(MPQHANDLE hMPQ) = MpqCompactArchive_stub;

MPQHANDLE (WINAPI* MpqOpenArchiveForUpdateEx)(LPCSTR lpFileName, DWORD dwFlags, DWORD dwMaximumFilesInArchive, DWORD dwBlockSize) = MpqOpenArchiveForUpdateEx_stub;
BOOL      (WINAPI* MpqAddFileToArchiveEx)(MPQHANDLE hMPQ, LPCSTR lpSourceFileName, LPCSTR lpDestFileName, DWORD dwFlags, DWORD dwCompressionType, DWORD dwCompressLevel) = MpqAddFileToArchiveEx_stub;
BOOL      (WINAPI* MpqAddFileFromBufferEx)(MPQHANDLE hMPQ, LPVOID lpBuffer, DWORD dwLength, LPCSTR lpFileName, DWORD dwFlags, DWORD dwCompressionType, DWORD dwCompressLevel) = MpqAddFileFromBufferEx_stub;
BOOL      (WINAPI* MpqAddFileFromBuffer)(MPQHANDLE hMPQ, LPVOID lpBuffer, DWORD dwLength, LPCSTR lpFileName, DWORD dwFlags) = MpqAddFileFromBuffer_stub;
BOOL      (WINAPI* MpqAddWaveFromBuffer)(MPQHANDLE hMPQ, LPVOID lpBuffer, DWORD dwLength, LPCSTR lpFileName, DWORD dwFlags, DWORD dwQuality) = MpqAddWaveFromBuffer_stub;
BOOL      (WINAPI* MpqRenameAndSetFileLocale)(MPQHANDLE hMPQ, LPCSTR lpcOldFileName, LPCSTR lpcNewFileName, LCID nOldLocale, LCID nNewLocale) = MpqRenameAndSetFileLocale_stub;
BOOL      (WINAPI* MpqDeleteFileWithLocale)(MPQHANDLE hMPQ, LPCSTR lpFileName, LCID nLocale) = MpqDeleteFileWithLocale_stub;
BOOL      (WINAPI* MpqSetFileLocale)(MPQHANDLE hMPQ, LPCSTR lpFileName, LCID nOldLocale, LCID nNewLocale) = MpqSetFileLocale_stub;

BOOL      (WINAPI* SFileDestroy)() = SFileDestroy_stub;
void      (WINAPI* StormDestroy)() = StormDestroy_stub;

#ifndef SFMPQAPI_DELAY_LOAD
SFMPQAPI_DELAY_LOADER::SFMPQAPI_DELAY_LOADER()
{
	LoadSFMpqDll();
}
#endif

SFMPQAPI_DELAY_LOADER::~SFMPQAPI_DELAY_LOADER()
{
	MpqInitialize = 0;

	MpqGetVersionString = 0;
	MpqGetVersion = 0;

	AboutSFMpq = 0;

	SFMpqGetVersionString = 0;
	SFMpqGetVersionString2 = 0;
	SFMpqGetVersion = 0;

	SFileOpenArchive = 0;
	SFileCloseArchive = 0;
	SFileOpenFileAsArchive = 0;
	SFileGetArchiveName = 0;
	SFileOpenFile = 0;
	SFileOpenFileEx = 0;
	SFileCloseFile = 0;
	SFileGetFileSize = 0;
	SFileGetFileArchive = 0;
	SFileGetFileName = 0;
	SFileSetFilePointer = 0;
	SFileReadFile = 0;
	SFileSetLocale = 0;
	SFileGetBasePath = 0;
	SFileSetBasePath = 0;

	SFileGetFileInfo = 0;
	SFileSetArchivePriority = 0;
	SFileFindMpqHeader = 0;
	SFileListFiles = 0;

	MpqOpenArchiveForUpdate = 0;
	MpqCloseUpdatedArchive = 0;
	MpqAddFileToArchive = 0;
	MpqAddWaveToArchive = 0;
	MpqRenameFile = 0;
	MpqDeleteFile = 0;
	MpqCompactArchive = 0;

	MpqOpenArchiveForUpdateEx = 0;
	MpqAddFileToArchiveEx = 0;
	MpqAddFileFromBufferEx = 0;
	MpqAddFileFromBuffer = 0;
	MpqAddWaveFromBuffer = 0;
	MpqRenameAndSetFileLocale = 0;
	MpqDeleteFileWithLocale = 0;
	MpqSetFileLocale = 0;

	SFileDestroy = 0;
	StormDestroy = 0;

	if (hSFMpq==0) return;
	if (SFMpqDestroy!=0) SFMpqDestroy();

	SFMpqDestroy = 0;

	FreeLibrary(hSFMpq);
	hSFMpq = 0;
}

long SFMpqCompareVersion()
{
	SFMPQVERSION ExeVersion = {1,0,8,1};
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

