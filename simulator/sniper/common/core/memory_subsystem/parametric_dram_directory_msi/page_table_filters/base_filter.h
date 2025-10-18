

#pragma once
#include "pagetable.h"
#include "core.h"

namespace ParametricDramDirectoryMSI
{
    class BaseFilter {
        
        public:

            // Returns true if the filter accepts the given address
            virtual PTWResult filterPTWResult(IntPtr virtual_address, PTWResult ptw_result, PageTable *page_table, bool count) = 0;

            BaseFilter(String _name, Core* _core):
                name(_name), core(_core) {}
            
            ~BaseFilter() = default;

        private:
            String name;
            Core* core;
    };
};