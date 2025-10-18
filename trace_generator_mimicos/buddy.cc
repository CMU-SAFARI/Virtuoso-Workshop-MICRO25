#include "buddy.h"
#include "fixed_types.h"
#include <vector>
#include <tuple>
#include <string>
#include <iostream>
#include <cmath>
#include <random>
#include <cassert>
#include <algorithm>
#define DEBUG_BUDDY 
using namespace std;

Buddy::Buddy(int memory_size, int max_order, int kernel_size, std::string frag_type) :
m_memory_size(memory_size), m_max_order(max_order), m_kernel_size(kernel_size), m_frag_type(frag_type)

{
	std::cout << "------ [Buddy] Initializing free lists ------" << std::endl;
	std::cout << std::endl;

    for (int i = 0; i < m_max_order + 1; i++)
	{
		std::vector<std::tuple<UInt64, UInt64, bool, UInt64>> vec; // Block start, Block end
		free_list.push_back(vec);
	}

    
	if (frag_type == "contiguity")
	{
		frag_fun = &Buddy::getAverageSizeRatio;
	}
	else if (frag_type == "largepage")
	{
		frag_fun = &Buddy::getLargePageRatio;
	}
	else
	{ // default is getLargePageRatio
		frag_fun = &Buddy::getLargePageRatio;
	}

	std::cout << "[Buddy] Memory Size: " << m_memory_size << std::endl;

	UInt64 pages_in_block = pow(2, m_max_order); // start with max

	UInt64 current_order = m_max_order;

	UInt64 total_mem_in_pages = m_memory_size * 1024 / 4 - m_kernel_size * 1024 / 4;

	m_total_pages = total_mem_in_pages;

	m_free_pages = m_total_pages;

	UInt64 available_mem_in_pages = total_mem_in_pages;

	UInt64 current_free = m_kernel_size * 1024 / 4;


	std::cout << "[Buddy] 4KB pages in memory: " << total_mem_in_pages << std::endl;
	std::cout << "[Buddy] 2MB pages in memory: " << total_mem_in_pages / 512 << std::endl;
	std::cout << "[Buddy] 1GB pages in memory: " << total_mem_in_pages / 512 / 512 << std::endl;

	while (current_free < (m_memory_size * 1024 / 4))
	{
		while (available_mem_in_pages >= pages_in_block)
		{
			std::cout << "[Buddy] Adding block of size " << pages_in_block << " at address " << current_free << std::endl; 
			free_list[current_order].push_back(std::make_tuple(current_free, current_free + pages_in_block - 1, false, -1));
			current_free += pages_in_block;
			available_mem_in_pages -= pages_in_block;
		}
#ifdef DEBUG_BUDDY_BUDDY_SIMPLE_THP
		std::cout << "[Buddy] Order " << current_order << " has " << free_list[current_order].size() << " blocks" << std::endl;
#endif
		current_order--;
		pages_in_block = pow(2, current_order);
	}
	std::cout << "[Buddy] Initialization done" << std::endl;

}

void Buddy::fragmentMemory(double target_fragmentation)
{

	std::vector<UInt64> used_pages;
	UInt64 chunk = 1;
	unsigned seed = 12345;
	std::mt19937 gen(seed);

	std::uniform_int_distribution<UInt64> dist(0, m_max_order - 4);

	int counter = 0;
	int declining_frag = 0;
	bool prev_declining = false;

	double current_fragmentation = (this->*frag_fun)();
	double prev_fragmentation = 1;

	std::cout << "[Artificial Fragmentation Generator] Current fragmentation: " << current_fragmentation << std::endl;

	while (current_fragmentation > target_fragmentation)
	{
		// Check if there is any large page to demote
		bool found = false;
		for (int i = m_max_order; i >= 9; i--)
		{
			if (free_list[i].size() > 0)
			{
				found = true;
				std::tuple<UInt64, UInt64, bool, UInt64> temp = free_list[i][0];
				free_list[i].erase(free_list[i].begin());

				UInt64 start = get<0>(temp);
				UInt64 end = get<1>(temp);
				UInt64 size = end - start + 1;
				// generate a random order between 8 and m_max_order-3, add seed
				std::uniform_int_distribution<UInt64> dist(8, i - 1);

				UInt64 random_order = dist(gen);
				// std::cout << "[Artificial Fragmentation Generator] Demoting to order: " << random_order << std::endl;
				UInt64 chunk = size / std::pow(2, random_order);
				UInt64 pages_in_block = pow(2, random_order);

				for (int j = 0; j < chunk; j++)
				{
					free_list[random_order].push_back(std::make_tuple(start + j * pages_in_block, start + (j + 1) * pages_in_block - 1, false, -1));
					assert(get<0>(free_list[random_order].back()) == (start + j * pages_in_block));
				}
				break;
			}
		}
		current_fragmentation = (this->*frag_fun)();
		std::cout << "[Artificial Fragmentation Generator] Current fragmentation: " << current_fragmentation << std::endl;
	}
	std::cout << "Initialized Buddy with final fragmentation: " << current_fragmentation << "and this ratio of memory: " << (double)(getFreePages() * 1.0 / m_total_pages * 1.0) << std::endl;
}


UInt64 Buddy::allocate(UInt64 bytes, UInt64 address, UInt64 core_id)
{

    int ind = ceil(log2(bytes / 4096));
    int i;

    for (i = ind; i <= m_max_order; i++)
    {
        if (free_list[i].size() != 0)
            break;
    }
    if (i == m_max_order + 1)
    {
        std::cout << "[Buddy] No free page inside memory" << std::endl;
    }
    else
    {
        std::tuple<UInt64, UInt64, bool, UInt64> temp;

        temp = free_list[i].back();
        free_list[i].pop_back();
        i--;


        while (i >= ind)
        {

            std::tuple<UInt64, UInt64, bool, UInt64> pair1, pair2;

            pair1 = std::make_tuple(get<0>(temp), get<0>(temp) + (get<1>(temp) - get<0>(temp)) / 2, false, -1);
            pair2 = std::make_tuple(get<0>(temp) + (get<1>(temp) - get<0>(temp)) / 2 + 1, get<1>(temp), false, -1);

            assert((get<1>(pair2) - get<0>(pair2) + 1) == pow(2, i));

            free_list[i].push_back(pair1);
            free_list[i].push_back(pair2);
            temp = free_list[i].back();
            free_list[i].pop_back();
            i--;
        }

        m_free_pages -= pow(2, ind);
        return get<0>(temp);
    }
	
}

std::tuple<UInt64, UInt64, bool, UInt64> Buddy::reserve_2mb_page(UInt64 address, UInt64 core_id)
{
	
	// check if there is a 2MB region available
#ifdef DEBUG_BUDDY
	std::cout << "DEBUG_BUDDY: Checking for 2MB region in free_list[9]" << std::endl;
#endif

	if (free_list[9].size() > 0)
	{

#ifdef DEBUG_BUDDY
	std::cout << "DEBUG_BUDDY: 2MB region available in free_list[9]" << std::endl;
#endif
		// get the 2MB region
		std::tuple<UInt64, UInt64, bool, UInt64> temp = free_list[9].back();
		free_list[9].pop_back();

#ifdef DEBUG_BUDDY
	std::cout << "DEBUG_BUDDY: Retrieved and removed 2MB region from free_list[9]" << std::endl;
#endif

		return temp;
	}
	else {

#ifdef DEBUG_BUDDY
	std::cout << "DEBUG_BUDDY: No 2MB region available in free_list[9], checking higher orders" << std::endl;
#endif

		for (int i = 10; i <= m_max_order; i++)
		{
			if (free_list[i].size() > 0)
			{

#ifdef DEBUG_BUDDY
			std::cout << "DEBUG_BUDDY: Region available in free_list[" << i << "]" << std::endl;
#endif
				std::tuple<UInt64, UInt64, bool, UInt64> temp = free_list[i].front();
				free_list[i].erase(free_list[i].begin());
				UInt64 start = get<0>(temp);
				UInt64 end = get<1>(temp);
				UInt64 size = end - start + 1;
				UInt64 chunk = size / 512;
				UInt64 pages_in_block = pow(2, 9);

	#ifdef DEBUG_BUDDY
				std::cout << "DEBUG_BUDDY: Splitting region from free_list[" << i << "] into 2MB chunks" << std::endl;
	#endif

				for (int j = 0; j < chunk; j++)
				{
					free_list[9].push_back(std::make_tuple(start + j * pages_in_block, start + (j + 1) * pages_in_block - 1, false, -1));
					assert(get<0>(free_list[9].back()) == (start + j * pages_in_block));
	#ifdef DEBUG_BUDDY
					std::cout << "DEBUG_BUDDY: Added 2MB chunk to free_list[9], start = " << (start + j * pages_in_block) << std::endl;
	#endif
				}
				temp = free_list[9].back();
				free_list[9].pop_back();
	#ifdef DEBUG_BUDDY
				std::cout << "DEBUG_BUDDY: Retrieved and removed 2MB region from free_list[9] after splitting" << std::endl;
				std::cout << "DEBUG_BUDDY: Returning 2MB region from 4KB start page: " << get<0>(temp) << " and end page: " << get<1>(temp) << std::endl;
	#endif

				return temp;
			}
		}
	}

	#ifdef DEBUG_BUDDY
		std::cout << "DEBUG_BUDDY: No region available, returning nullptr" << std::endl;
	#endif

	return std::make_tuple(-1, 0, false, 0);

}


void Buddy::free(UInt64 start, UInt64 end)
{
	int order = ceil(log2((end - start + 1)));
	int i = order;

	#ifdef DEBUG_BUDDY
	//std::cout << "Debug: Calculated order = " << order << std::endl;
	#endif
	// We assume that the freed block is a power of 2
	std::tuple<UInt64, UInt64, bool, UInt64> temp = std::make_tuple(start, end, false, -1);

	free_list[i].push_back(temp);

	#ifdef DEBUG_BUDDY
	//std::cout << "Debug: Pushed tuple to free_list[" << i << "]" << std::endl;
	#endif

	m_free_pages += pow(2, i);

	#ifdef DEBUG_BUDDY
	//std::cout << "Debug: Updated m_free_pages = " << m_free_pages << std::endl;
	#endif
}


double Buddy::getAverageSizeRatio()
{
	std::vector<UInt64> blockSizes;

	// Collect sizes of all free blocks
	for (const auto &list : free_list)
	{
		for (const auto &block : list)
		{
			// check if the block is contiguous
			blockSizes.push_back(get<1>(block) - get<0>(block) + 1);
		}
	}

	// Sort block sizes in descending order
	std::sort(blockSizes.rbegin(), blockSizes.rend());

	// Calculate average of top 50 blocks
	UInt64 totalSize = 0;
	int count = 0;
	for (auto size : blockSizes)
	{
		totalSize += size;
		count++;
		if (count == 50)
			break;
	}

	double averageSize = (count > 0) ? (double)totalSize / count : 0.0;
	return averageSize / pow(2, m_max_order - 3);
}

double Buddy::getLargePageRatio()
{
	int numberOfLargePages = 0;

	// Collect number of 2MB pages based on the free list
	for (const auto &list : free_list)
	{
		for (const auto &block : list)
		{
			if ((get<1>(block) - get<0>(block) + 1) >= 512)
			{
				numberOfLargePages += (get<1>(block) - get<0>(block) + 1) / 512;
			}
		}
	}

	// calculate the ratio of available large pages to the total number of 2MB pages
	double largePageRatio = (double)numberOfLargePages / (m_total_pages / 512);
	m_frag_factor = largePageRatio;
	return largePageRatio;
}