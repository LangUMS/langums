#ifndef __HASHUTIL_H
#define __HASHUTIL_H

#include <string>
#include <fstream>
#include <vector>

#include "keccak.h"

namespace Langums
{
    bool GetFileHash(const std::string& path, std::string& outHash)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }

        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        Keccak hash;
        outHash = hash(bytes.data(), bytes.size());
        return true;
    }
}

#endif
