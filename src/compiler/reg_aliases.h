#ifndef __LANGUMS_REG_ALIASES_H
#define __LANGUMS_REG_ALIASES_H

#include <string>
#include <unordered_map>

#include "../ast/ast.h"
#include "ir_exceptions.h"

namespace Langums
{

    class RegisterAliases
    {
        public:
        RegisterAliases()
        {}

        bool HasAlias(const std::string& name, unsigned int index, IASTNode* node) const
        {
            auto fn = FindFunctionDeclarationForNode(node);
            if (fn == nullptr)
            {
                return HasGlobalAlias(name, index);
            }

            auto it = m_Aliases.find(fn);
            if (it == m_Aliases.end())
            {
                return HasGlobalAlias(name, index);
            }

            auto it2 = (*it).second.find(name);
            if (it2 == (*it).second.end())
            {
                return HasGlobalAlias(name, index);
            }

            auto& registers = (*it2).second;
            if (index >= registers.size())
            {
                if (registers.size() == 1)
                {
                    return false;
                }
                else
                {
                    return false;
                }
            }

            return true;
        }

        bool HasGlobalAlias(const std::string& name, unsigned int index) const
        {
            auto it = m_GlobalAliases.find(name);
            if (it == m_GlobalAliases.end())
            {
                return false;
            }

            auto& registers = (*it).second;
            if (index >= registers.size())
            {
                if (registers.size() == 1)
                {
                    return false;
                }
                else
                {
                    return false;
                }
            }

            return true;
        }

        int GetAlias(const std::string& name, unsigned int index, IASTNode* node) const
        {
            auto fn = FindFunctionDeclarationForNode(node);
            if (fn == nullptr)
            {
                return GetGlobalAlias(name, index);
            }

            auto it = m_Aliases.find(fn);
            if (it == m_Aliases.end())
            {
                return GetGlobalAlias(name, index);
            }

            auto it2 = (*it).second.find(name);
            if (it2 == (*it).second.end())
            {
                return GetGlobalAlias(name, index);
            }

            auto& registers = (*it2).second;
            if (index >= registers.size())
            {
                if (registers.size() == 1)
                {
                    throw IRCompilerException(SafePrintf("Invalid register name \"%\"", name), node);
                }
                else
                {
                    throw IRCompilerException(SafePrintf("Array access out of bounds for \"%[%]\"", name, index), node);
                }
            }

            return Reg_ReservedEnd + registers[index];
        }

        int GetGlobalAlias(const std::string& name, unsigned int index) const
        {
            auto it = m_GlobalAliases.find(name);
            if (it == m_GlobalAliases.end())
            {
                throw IRCompilerException(SafePrintf("Invalid register name \"%\"", name), nullptr);
            }

            auto& registers = (*it).second;
            if (index >= registers.size())
            {
                if (registers.size() == 1)
                {
                    throw IRCompilerException(SafePrintf("Invalid register name \"%\"", name), nullptr);
                }
                else
                {
                    throw IRCompilerException(SafePrintf("Array access out of bounds for \"%[%]\"", name, index), nullptr);
                }
            }

            return Reg_ReservedEnd + registers[index];
        }

        void Allocate(const std::string& name, unsigned int count, IASTNode* node)
        {
            auto fn = FindFunctionDeclarationForNode(node);
            if (fn == nullptr)
            {
                auto& registers = m_GlobalAliases[name];

                for (auto& id : registers)
                {
                    m_FreeIds.push_back(id);
                }

                registers.clear();

                for (auto i = 0u; i < count; i++)
                {
                    registers.push_back(GetNextFreeId());
                }

                return;
            }

            auto& fnAliases = m_Aliases[fn];
            auto& registers = fnAliases[name];

            for (auto& id : registers)
            {
                m_FreeIds.push_back(id);
            }

            registers.clear();

            for (auto i = 0u; i < count; i++)
            {
                registers.push_back(GetNextFreeId());
            }
        }

        void Deallocate(const std::string& name, IASTNode* node)
        {
            auto fn = FindFunctionDeclarationForNode(node);
            if (fn == nullptr)
            {
                auto& registers = m_GlobalAliases[name];
                for (auto id : registers)
                {
                    m_FreeIds.push_back(id);
                }

                m_GlobalAliases.erase(name);
                return;
            }

            auto& fnAliases = m_Aliases[fn];
            auto& registers = fnAliases[name];
            for (auto id : registers)
            {
                m_FreeIds.push_back(id);
            }

            fnAliases.erase(name);
        }

        const std::unordered_map<std::string, std::vector<unsigned int>>& GetAliases(IASTNode* node) const
        {
            auto fn = FindFunctionDeclarationForNode(node);
            if (fn == nullptr)
            {
                return m_GlobalAliases;
            }

            auto it = m_Aliases.find(fn);
            if (it == m_Aliases.end())
            {
                return m_DummyEmptyAliases;
            }

            return (*it).second;
        }

        private:
        unsigned int GetNextFreeId()
        {
            if (m_FreeIds.size() > 0)
            {
                auto next = m_FreeIds.back();
                m_FreeIds.pop_back();
                return next;
            }

            return m_NextFreeId++;
        }

        ASTFunctionDeclaration* FindFunctionDeclarationForNode(IASTNode* node) const
        {
            while (node != nullptr)
            {
                if (node->GetType() == ASTNodeType::FunctionDeclaration)
                {
                    return (ASTFunctionDeclaration*)node;
                }

                node = node->GetParent();
            }

            return nullptr;
        }

        std::unordered_map<ASTFunctionDeclaration*, std::unordered_map<std::string, std::vector<unsigned int>>> m_Aliases;
        std::unordered_map<std::string, std::vector<unsigned int>> m_GlobalAliases;
        std::unordered_map<std::string, std::vector<unsigned int>> m_DummyEmptyAliases;
        std::vector<unsigned int> m_FreeIds;
        unsigned int m_NextFreeId = 0;
    };

}

#endif
