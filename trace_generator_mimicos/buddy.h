#pragma once

#include <vector>
#include <tuple>
#include <string>
#include "fixed_types.h"

class Buddy{

public:

    Buddy(int memory_size, int max_order, int kernel_size, std::string frag_type);
    void fragmentMemory(double target_fragmentation);
    UInt64 allocate(UInt64 size, UInt64 address, UInt64 core_id);
    std::tuple<UInt64, UInt64, bool, UInt64> reserve_2mb_page(UInt64 address, UInt64 core_id);
    void free(UInt64 start, UInt64 end);

    double getAverageSizeRatio();
    double getLargePageRatio();
    int getFreePages() { return m_free_pages; }
    int getTotalPages() { return m_total_pages; }


private: 
    int m_memory_size;
    int m_kernel_size;
    int m_max_order;
    double m_frag_factor;
    std::string m_frag_type;
    UInt64 m_total_pages;
    UInt64 m_free_pages;
    std::vector<std::vector<std::tuple<UInt64, UInt64, bool, UInt64>>> free_list;

    double (Buddy::*frag_fun)();

};