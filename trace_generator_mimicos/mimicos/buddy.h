#pragma once
#include "fixed_types.h"
#include <vector>
#include <tuple>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <string>
#include <random>

#include "debug_config.h"
using namespace std;
template <typename Policy>
class Buddy : private Policy
{
public:
    Buddy(int memory_size, int max_order, int kernel_size, const String &frag_type)
        : m_memory_size(memory_size),
          m_max_order(max_order),
          m_kernel_size(kernel_size),
          m_frag_type(frag_type)
    {

        Policy::on_init(m_memory_size, m_max_order, m_kernel_size);

#if DEBUG_BUDDY >= DEBUG_BASIC
        // Initialize free lists
        this->log("Debug: Initializing Buddy allocator with memory_size = " + std::to_string(memory_size) +
                  ", max_order = " + std::to_string(max_order) +
                  ", kernel_size = " + std::to_string(kernel_size) +
                  ", frag_type = " + frag_type.c_str());
#endif
        free_list.resize(m_max_order + 1);
        for (int i = 0; i < m_max_order + 1; i++)
            free_list.emplace_back();

        if (frag_type == "contiguity")
            frag_fun = &Buddy::getAverageSizeRatio;
        else
            frag_fun = &Buddy::getLargePageRatio;

        std::cout << "[Buddy] Memory Size: " << m_memory_size << std::endl;
        UInt64 pages_in_block = 1ULL << m_max_order;
        UInt64 current_order = m_max_order;

        UInt64 total_mem_in_pages = static_cast<UInt64>(m_memory_size) * 1024 / 4 - static_cast<UInt64>(m_kernel_size) * 1024 / 4;
        m_total_pages = total_mem_in_pages;
        m_free_pages = m_total_pages;

        UInt64 available_mem_in_pages = total_mem_in_pages;
        UInt64 current_free = static_cast<UInt64>(m_kernel_size) * 1024 / 4;

        std::cout << "[Buddy] 4KB pages in memory: " << total_mem_in_pages << std::endl;
        std::cout << "[Buddy] 2MB pages in memory: " << total_mem_in_pages / 512 << std::endl;
        std::cout << "[Buddy] 1GB pages in memory: " << total_mem_in_pages / 512 / 512 << std::endl;
        while (current_free < (static_cast<UInt64>(m_memory_size) * 1024 / 4))
        {
            while (available_mem_in_pages >= pages_in_block)
            {
#if DEBUG_BUDDY >= DEBUG_DETAILED
                this->log("[Buddy] Adding block of size " + std::to_string(pages_in_block) +
                          " at address " + std::to_string(current_free));
#endif
                free_list[current_order].push_back(std::make_tuple(current_free, current_free + pages_in_block - 1, false, -1));
                current_free += pages_in_block;
                available_mem_in_pages -= pages_in_block;
            }
#if DEBUG_BUDDY >= DEBUG_BASIC
            this->log("[Buddy] Order " + std::to_string(current_order) +
                      " has " + std::to_string(free_list[current_order].size()) + " blocks");
#endif
            current_order--;
            pages_in_block = 1ULL << current_order;
        }
    }

    UInt64 allocate(UInt64 bytes, UInt64 address = 0, UInt64 core_id = -1)
    {
        int ind = ceil(log2(bytes / 4096));
        int i;
        for (i = ind; i <= m_max_order; i++)
            if (!free_list[i].empty())
                break;

        if (i == m_max_order + 1)
        {
            Policy::on_out_of_memory(bytes, address, core_id);
            return static_cast<UInt64>(-1);
        }

        auto block = free_list[i].back();
#if DEBUG_BUDDY >= DEBUG_BASIC
        this->log("[Buddy] Found free page in order " + std::to_string(i));
#endif
        free_list[i].pop_back();
        i--;

        while (i >= ind)
        {
            auto pair1 = std::make_tuple(get<0>(block), get<0>(block) + (get<1>(block) - get<0>(block)) / 2, false, -1);
            auto pair2 = std::make_tuple(get<0>(block) + (get<1>(block) - get<0>(block)) / 2 + 1, get<1>(block), false, -1);
            assert((get<1>(pair2) - get<0>(pair2) + 1) == (1ULL << i));
            free_list[i].push_back(pair1);
            free_list[i].push_back(pair2);
            block = free_list[i].back();
            free_list[i].pop_back();
            i--;
        }

#if DEBUG_BUDDY >= DEBUG_BASIC
        this->log("[Buddy] Allocated " + std::to_string(bytes) +
                  " bytes at address " + std::to_string(get<0>(block)));
#endif
        m_free_pages -= (1ULL << ind);
        return get<0>(block);
    }

    std::tuple<UInt64, UInt64, bool, UInt64> reserve_2mb_page(UInt64 address, UInt64 core_id)
    {
        // check if there is a 2MB region available
#if DEBUG_BUDDY >= DEBUG_BASIC
        this->log("DEBUG_BUDDY: Checking for 2MB region in free_list[9]");
#endif
        if (!free_list[9].empty())
        {
#if DEBUG_BUDDY >= DEBUG_BASIC
            this->log("DEBUG_BUDDY: 2MB region available in free_list[9]");
#endif
            auto block = free_list[9].back();
            free_list[9].pop_back();
#if DEBUG_BUDDY >= DEBUG_BASIC
            this->log("DEBUG_BUDDY: Retrieved and removed 2MB region from free_list[9]");
#endif
            return block;
        }

#if DEBUG_BUDDY >= DEBUG_BASIC
        this->log("DEBUG_BUDDY: No 2MB region available in free_list[9], checking higher orders");
#endif
        for (int i = 10; i <= m_max_order; i++)
        {
            if (!free_list[i].empty())
            {
#if DEBUG_BUDDY >= DEBUG_BASIC
                this->log("DEBUG_BUDDY: Region available in free_list[" + std::to_string(i) + "]");
#endif
                auto temp = free_list[i].front();
                free_list[i].erase(free_list[i].begin());

                UInt64 start = get<0>(temp);
                UInt64 end = get<1>(temp);
                UInt64 size = end - start + 1;
                UInt64 chunk = size / 512;
                UInt64 pages_in_block = 1ULL << 9;

#if DEBUG_BUDDY >= DEBUG_BASIC
                this->log("DEBUG_BUDDY: Splitting region from free_list[" + std::to_string(i) + "] into 2MB chunks");
#endif

                for (UInt64 j = 0; j < chunk; j++)
                {
                    free_list[9].push_back(std::make_tuple(start + j * pages_in_block, start + (j + 1) * pages_in_block - 1, false, -1));
                    assert(get<0>(free_list[9].back()) == (start + j * pages_in_block));
#if DEBUG_BUDDY >= DEBUG_BASIC
                    this->log("DEBUG_BUDDY: Added 2MB chunk to free_list[9], start = " + std::to_string(start + j * pages_in_block));
#endif
                }

                auto block = free_list[9].back();
                free_list[9].pop_back();
#if DEBUG_BUDDY >= DEBUG_BASIC
                this->log("DEBUG_BUDDY: Retrieved and removed 2MB region from free_list[9] after splitting");
                this->log("DEBUG_BUDDY: Returning 2MB region from 4KB start page: " + std::to_string(get<0>(temp)) + " and end page: " + std::to_string(get<1>(temp)));
#endif
                return block;
            }
        }

#if DEBUG_BUDDY >= DEBUG_BASIC
        this->log("DEBUG_BUDDY: No region available, returning nullptr");
#endif
        return std::make_tuple((UInt64)-1, 0, false, 0);
    }

    void free(UInt64 start, UInt64 end)
    {
        int order = ceil(log2((end - start + 1)));
        int i = order;
#if DEBUG_BUDDY >= DEBUG_BASIC
        this->log("Debug: Calculated order = " + std::to_string(order));
#endif

        std::tuple<UInt64, UInt64, bool, UInt64> temp = std::make_tuple(start, end, false, -1);

#if DEBUG_BUDDY >= DEBUG_BASIC
        this->log("Debug: Created tuple with start = " +
                  std::to_string(start) +
                  " and end = " +
                  std::to_string(end));
#endif

        free_list[i].push_back(temp);

#if DEBUG_BUDDY >= DEBUG_BASIC
        this->log("Debug: Pushed tuple to free_list[" +
                  std::to_string(i) +
                  "]");
#endif

        m_free_pages += (1ULL << i);

#if DEBUG_BUDDY >= DEBUG_BASIC
        this->log("Debug: Updated m_free_pages = " + std::to_string(m_free_pages));
#endif
    }

    void fragmentMemory(double target_fragmentation)
    {
        std::cout << "[BUDDY] Fragmenting memory to achieve target fragmentation: " << target_fragmentation << std::endl;
        std::mt19937 gen(12345);
        double current_fragmentation = (this->*frag_fun)();
        while (current_fragmentation > target_fragmentation)
        {
            for (int i = m_max_order; i >= 9; i--)
            {
                if (!free_list[i].empty())
                {
                    auto temp = free_list[i][0];
                    free_list[i].erase(free_list[i].begin());

                    UInt64 start = get<0>(temp);
                    UInt64 end = get<1>(temp);
                    UInt64 size = end - start + 1;
                    // generate a random order between 8 and m_max_order-3, add seed
                    std::uniform_int_distribution<UInt64> dist(8, i - 1);

                    UInt64 random_order = dist(gen);
                    UInt64 chunk = size / (1ULL << random_order);
                    UInt64 pages_in_block = 1ULL << random_order;

                    // Essentially, we are splitting the large page into smaller pages
                    // and adding them to the free list
                    // This way we are increasing the fragmentation without actually allocating memory which is very useful for testing and evaluation
                    for (UInt64 j = 0; j < chunk; j++)
                    {
                        free_list[random_order].push_back(std::make_tuple(start + j * pages_in_block, start + (j + 1) * pages_in_block - 1, false, -1));
                        assert(get<0>(free_list[random_order].back()) == (start + j * pages_in_block));
                    }

                    break;
                }
            }

            current_fragmentation = (this->*frag_fun)();
#if DEBUG_BUDDY >= DEBUG_DETAILED
            this->log("[Artificial Fragmentation Generator] Current fragmentation: " + std::to_string(current_fragmentation));
#endif
        }

        std::cout << "[BUDDY] Initialized memory with final fragmentation: " << current_fragmentation << std::endl;
        std::cout << "[BUDDY] Free pages: " << m_free_pages << std::endl;
        std::cout << "[BUDDY] Free pages in (MB): " << m_free_pages * 4 / 1024 << std::endl;
        Policy::on_fragmentation_done();
    }

    double getAverageSizeRatio()
    {
        std::vector<UInt64> blockSizes;

        // Collect sizes of all free blocks
        for (const auto &list : free_list)
            for (const auto &block : list)
                blockSizes.push_back(get<1>(block) - get<0>(block) + 1); // check if the block is contiguous

        // Sort block sizes in descending order
        std::sort(blockSizes.rbegin(), blockSizes.rend());

        // Calculate average of top 50 blocks
        UInt64 totalSize = 0;
        int count = 0;
        for (auto size : blockSizes)
        {
            totalSize += size;
            if (++count == 50)
                break;
        }

        double averageSize = (count > 0) ? (double)totalSize / count : 0.0;
        return averageSize / (1ULL << (m_max_order - 3));
    }

    double getLargePageRatio()
    {
        int numberOfLargePages = 0;
        // Collect number of 2MB pages based on the free list
        for (const auto &list : free_list)
            for (const auto &block : list)
                if ((get<1>(block) - get<0>(block) + 1) >= 512)
                    numberOfLargePages += (get<1>(block) - get<0>(block) + 1) / 512;

        // calculate the ratio of available large pages to the total number of 2MB pages
        m_frag_factor = (double)numberOfLargePages / (m_total_pages / 512);
        // std::cout << "[BUDDY] Large page ratio: " << m_frag_factor << std::endl;
        return m_frag_factor;
    }

    UInt64 getFreePages() const { return m_free_pages; }
    UInt64 getTotalPages() const { return m_total_pages; }

private:
    int m_memory_size;
    int m_max_order;
    int m_kernel_size;
    String m_frag_type;
    double m_frag_factor;

    UInt64 m_total_pages;
    UInt64 m_free_pages;

    std::vector<std::vector<std::tuple<UInt64, UInt64, bool, UInt64>>> free_list;
    double (Buddy::*frag_fun)();
};
