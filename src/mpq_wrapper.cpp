#include <vector>

#include "stormlib/StormLib.h"

#include "stringutil.h"
#include "mpq_wrapper.h"

namespace Langums
{

    MPQWrapper::MPQWrapper(const std::string& path, bool readOnly) :
        m_Path(path), m_ReadOnly(readOnly)
    {
        auto flags = 0;
        if (readOnly)
        {
            flags = MPQ_OPEN_READ_ONLY;
        }

        if (!SFileOpenArchive(path.c_str(), 0, flags, &m_MPQ))
        {
            throw MPQWrapperException(SafePrintf("SFileOpenArchive failed, error code: %. Looks like an invalid scx file.", GetLastError()));
        }
    }

    MPQWrapper::~MPQWrapper()
    {
        SFileCloseArchive(m_MPQ);
    }

    void MPQWrapper::ReadFile(const std::string& filename, std::vector<char>& retBytes)
    {
        HANDLE file = nullptr;
        if (!SFileOpenFileEx(m_MPQ, filename.c_str(), 0, &file) || file == nullptr)
        {
            throw MPQWrapperException(SafePrintf("Failed to find \"%\" in MPQ, not a Brood War map file?", filename));
        }

        auto fileSize = SFileGetFileSize(file, 0);
        retBytes.resize(fileSize);

        if (!SFileReadFile(file, retBytes.data(), fileSize, 0, 0))
        {
            throw MPQWrapperException(SafePrintf("SFileReadFile failed for \"%\".", filename));
        }

        SFileCloseFile(file);
    }

    void MPQWrapper::WriteFile(const std::string& filename, const std::vector<char>& bytes, bool compress)
    {
        auto fileFlags = MPQ_FILE_REPLACEEXISTING | MPQ_FILE_COMPRESS;
        if (!compress)
        {
            fileFlags = MPQ_FILE_REPLACEEXISTING;
        }

        HANDLE file = nullptr;
        if (!SFileCreateFile(m_MPQ, filename.c_str(), 0, bytes.size(), 0, fileFlags, &file))
        {
            throw MPQWrapperException(SafePrintf("Failed to open \"%\" for writing.", filename));
        }

        if (!SFileWriteFile(file, bytes.data(), bytes.size(), compress ? MPQ_COMPRESSION_PKWARE : 0))
        {
            throw MPQWrapperException(SafePrintf("Failed to write out \"%\" to MPQ archive.", filename));
        }
    }

    void MPQWrapper::AddWavFile(const std::string& path, const std::string& mpqFilename)
    {
        if (!SFileAddWave(m_MPQ, path.c_str(), mpqFilename.c_str(), MPQ_COMPRESSION_ADPCM_STEREO, MPQ_WAVE_QUALITY_HIGH))
        {
            throw MPQWrapperException(SafePrintf("Failed to add \"%\" to the MPQ archive.", path));
        }
    }

}
