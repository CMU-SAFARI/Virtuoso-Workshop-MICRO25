
#pragma once
#include "base_filter.h"


namespace ParametricDramDirectoryMSI
{
    class RadixFilter : public BaseFilter {

        public:
            RadixFilter(String _name, Core* _core);
            ~RadixFilter();

            PTWResult filterPTWResult(IntPtr virtual_address, PTWResult ptw_result, PageTable *page_table, bool count);

            void setPWC(PWC *_pwc) {
                pwc = _pwc;
                m_pwc_enabled = true;
            }
        private:
            PWC *pwc; // Only used for radix page tables
            bool m_pwc_enabled;
            int max_pwc_level;
            
    };
};