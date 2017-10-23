#ifndef __FILEUTIL_H
#define __FILEUTIL_H

#include <string>
#include <fstream>
#include <vector>

namespace Langums
{

    bool ReadTextFile(const std::string& path, std::string& outContents)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return false;
        }

        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        outContents.resize(bytes.size());
        memcpy((void*)outContents.data(), bytes.data(), bytes.size());
        return true;
    }

    bool WriteTextFile(const std::string& path, const std::string& contents)
    {
        std::ofstream file(path);
        if (!file.is_open())
        {
            return false;
        }

        file << contents;
        file.flush();
        return true;
    }

}

#endif
