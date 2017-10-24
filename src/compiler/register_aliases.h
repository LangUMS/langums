#ifndef __LANGUMS_REG_ALIASES_H
#define __LANGUMS_REG_ALIASES_H

#include <string>
#include <unordered_map>

namespace LangUMS
{

    class IASTNode;
    class ASTFunctionDeclaration;

    class RegisterAliases
    {
        public:
        bool HasAlias(const std::string& name, unsigned int index, IASTNode* node) const;
        bool HasGlobalAlias(const std::string& name, unsigned int index) const;
        int GetAlias(const std::string& name, unsigned int index, IASTNode* node) const;
        int GetGlobalAlias(const std::string& name, unsigned int index) const;
        void Allocate(const std::string& name, unsigned int count, IASTNode* node);
        void Deallocate(const std::string& name, IASTNode* node);
        const std::unordered_map<std::string, std::vector<unsigned int>>& GetAliases(IASTNode* node) const;

        private:
        unsigned int GetNextFreeId();

        std::unordered_map<ASTFunctionDeclaration*, std::unordered_map<std::string, std::vector<unsigned int>>> m_Aliases;
        std::unordered_map<std::string, std::vector<unsigned int>> m_GlobalAliases;
        std::unordered_map<std::string, std::vector<unsigned int>> m_DummyEmptyAliases;
        std::vector<unsigned int> m_FreeIds;
        unsigned int m_NextFreeId = 0;
    };

}

#endif
