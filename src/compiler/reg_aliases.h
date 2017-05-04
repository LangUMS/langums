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

        bool HasAlias(const std::string& name, unsigned int index) const
        {
            auto it = m_Aliases.find(name);
            if (it == m_Aliases.end())
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
            auto it = m_Aliases.find(name);
            if (it == m_Aliases.end())
            {
                throw IRCompilerException(SafePrintf("Invalid register name \"%\"", name), node);
            }

            auto& registers = (*it).second;
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

        void Allocate(const std::string& name, unsigned int count)
        {
            auto& registers = m_Aliases[name];

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

        void Deallocate(const std::string& name)
        {
            auto& registers = m_Aliases[name];
            for (auto id : registers)
            {
                m_FreeIds.push_back(id);
            }

            m_Aliases.erase(name);
        }

        const std::unordered_map<std::string, std::vector<unsigned int>>& GetAliases() const
        {
            return m_Aliases;
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

        std::unordered_map<std::string, std::vector<unsigned int>> m_Aliases;
        std::vector<unsigned int> m_FreeIds;
        unsigned int m_NextFreeId = 0;
    };

}

#endif
