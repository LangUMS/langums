#include <functional>
#include <iterator>
#include <algorithm>

#include "../stringutil.h"
#include "../ast/ast.h"

#include "ir_constants.h"
#include "ir_exceptions.h"
#include "ir_utility.h"

#include "register_aliases.h"

namespace LangUMS
{

    bool RegisterAliases::HasAlias(const std::string& name, unsigned int index, IASTNode* node) const
    {
        auto fn = FindFunctionDeclarationForNode(node);
        if (fn == nullptr)
        {
            return HasGlobalAlias(name, index);
        }

        auto localAliases = m_Aliases.find(fn);
        if (localAliases == m_Aliases.end())
        {
            return HasGlobalAlias(name, index);
        }

        auto registerList = localAliases->second.find(name);
        if (registerList == localAliases->second.end())
        {
            return HasGlobalAlias(name, index);
        }

        auto& registers = registerList->second;
        return index < registers.size();
    }

    bool RegisterAliases::HasGlobalAlias(const std::string& name, unsigned int index) const
    {
        auto registerList = m_GlobalAliases.find(name);
        if (registerList == m_GlobalAliases.end())
        {
            return false;
        }

        auto& registers = registerList->second;
        return index < registers.size();
    }

    int RegisterAliases::GetAlias(const std::string& name, unsigned int index, IASTNode* node) const
    {
        auto fn = FindFunctionDeclarationForNode(node);
        if (fn == nullptr)
        {
            return GetGlobalAlias(name, index);
        }

        auto localAliases = m_Aliases.find(fn);
        if (localAliases == m_Aliases.end())
        {
            return GetGlobalAlias(name, index);
        }

        auto registerList = localAliases->second.find(name);
        if (registerList == localAliases->second.end())
        {
            return GetGlobalAlias(name, index);
        }

        auto& registers = registerList->second;
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

    int RegisterAliases::GetGlobalAlias(const std::string& name, unsigned int index) const
    {
        auto registerList = m_GlobalAliases.find(name);
        if (registerList == m_GlobalAliases.end())
        {
            throw IRCompilerException(SafePrintf("Invalid register name \"%\"", name), nullptr);
        }

        auto& registers = registerList->second;
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

    void RegisterAliases::Allocate(const std::string& name, unsigned int count, IASTNode* node)
    {
        auto fn = FindFunctionDeclarationForNode(node);
        if (fn == nullptr)
        {
            auto& registers = m_GlobalAliases[name];
            m_FreeIds.insert(m_FreeIds.end(), registers.begin(), registers.end());
            registers.resize(count);
            std::generate(registers.begin(), registers.end(), std::bind(&RegisterAliases::GetNextFreeId, this));
            return;
        }

        auto& localAliases = m_Aliases[fn];
        auto& registers = localAliases[name];

        m_FreeIds.insert(m_FreeIds.end(), registers.begin(), registers.end());
        registers.resize(count);
        std::generate(registers.begin(), registers.end(), std::bind(&RegisterAliases::GetNextFreeId, this));
    }

    void RegisterAliases::Deallocate(const std::string& name, IASTNode* node)
    {
        auto fn = FindFunctionDeclarationForNode(node);
        if (fn == nullptr)
        {
            auto& registers = m_GlobalAliases[name];
            m_FreeIds.insert(m_FreeIds.end(), registers.begin(), registers.end());
            m_GlobalAliases.erase(name);
            return;
        }

        auto& localAliases = m_Aliases[fn];
        auto& registers = localAliases[name];

        m_FreeIds.insert(m_FreeIds.end(), registers.begin(), registers.end());
        localAliases.erase(name);
    }

    const std::unordered_map<std::string, std::vector<unsigned int>>& RegisterAliases::GetAliases(IASTNode* node) const
    {
        auto fn = FindFunctionDeclarationForNode(node);
        if (fn == nullptr)
        {
            return m_GlobalAliases;
        }

        auto localAliases = m_Aliases.find(fn);
        if (localAliases == m_Aliases.end())
        {
            return m_DummyEmptyAliases;
        }

        return localAliases->second;
    }

    unsigned int RegisterAliases::GetNextFreeId()
    {
        if (m_FreeIds.size() > 0)
        {
            auto next = m_FreeIds.back();
            m_FreeIds.pop_back();
            return next;
        }

        return m_NextFreeId++;
    }

}
