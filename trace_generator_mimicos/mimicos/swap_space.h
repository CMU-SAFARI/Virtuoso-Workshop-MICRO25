#ifndef SWAP_SPACE_H
#define SWAP_SPACE_H

#include <string>
#include <unordered_map>
#include <list>
#include <fstream>
#include <cstddef> // Required for size_t
#include <vector>
#include "fixed_types.h"

// Represents the unique location of a page in the backing store (swap disk)

using namespace std;

/**
 * @brief A class that imitates the Linux swap cache.
 */
class SwapCache {

private:
    int swap_size;
    
    // Custom hash function for std::pair<IntPtr, int>
    struct PairHash {
        std::size_t operator()(const std::pair<IntPtr, int>& p) const {
        return std::hash<IntPtr>()(p.first) ^ (std::hash<int>()(p.second) << 1);
     }
    };

    // Custom equality comparator (optional if std::pair's operator== suffices)
    struct PairEqual {
        bool operator()(const std::pair<IntPtr, int>& lhs, const std::pair<IntPtr, int>& rhs) const {
        return lhs.first == rhs.first && lhs.second == rhs.second;
     }
    };

    // Define the unordered_map
    std::unordered_map<std::pair<IntPtr, int>, IntPtr, PairHash, PairEqual> swap_cache_map;

    // Define a data structure that stores free pages in the swap cache
    std::vector<bool> free_pages;

public:
    SwapCache(int swap_size_mb) {
        swap_size = swap_size_mb * 1024 / 4;
        free_pages.resize(swap_size, true); // All pages are initially free
        stats.swap_ins = 0;
        stats.swap_outs = 0;
        stats.failed_swap_outs_space = 0;
    }

    // Access a page, triggering a major page fault if not in RAM.
    bool lookup(IntPtr page_id, int app_id) {
        // Check if the page is in the swap cache
        auto it = swap_cache_map.find(make_pair(page_id, app_id));

        if (it != swap_cache_map.end()) {
            // Page found in swap cache, update LRU list
            return true; // Page is in swap
        } else {
            // Here you would typically handle the page fault, e.g., loading the page from disk
            return false; // Page is not in RAM
        }
    }

    std::tuple<bool, IntPtr> swapOut(IntPtr virtual_page, int app_id, bool is_memory_full) {
        // Add the page to the swap cache
        IntPtr swap_location = findFreePage();

        if (swap_location == static_cast<IntPtr>(-1)) {
            return std::make_tuple(false, static_cast<IntPtr>(-1)); // Cannot swap out
        }

        swap_cache_map[std::make_pair(virtual_page, app_id)] = swap_location;

        // If the cache exceeds a certain size, remove the least recently used page
        // We attempted to swap out a page and the memory is not full but the swap is full
        int current_size = swap_cache_map.size();
        if ( current_size > swap_size) {
            stats.failed_swap_outs_space++;
            return std::make_tuple(false, static_cast<IntPtr>(-1)); // Swap out failed
        }

        // Successfully swapped out the page
        stats.swap_outs++;
        return std::make_tuple(true, swap_location);
    }


    // This function would handle swapping a page into RAM from the swap cache
    IntPtr swapIn(IntPtr virtual_page, int app_id){
        IntPtr swap_location = swap_cache_map[make_pair(virtual_page, app_id)];

        // Remove the page from the swap cache
        swap_cache_map.erase(make_pair(virtual_page, app_id));
        stats.swap_ins++;

        return swap_location;
    }

    IntPtr findFreePage() {
        for (size_t i = 0; i < free_pages.size(); ++i) {
            if (free_pages[i]) {
                free_pages[i] = false; // Mark this page as used
                return i; // Return the index as the page ID
            }
        }
        #ifdef DEBUG
            log_file<< "[SWAP_CACHE] No free page found." << std::endl;
        #endif
        return  static_cast<IntPtr>(-1); // No free page found        
    }


    struct {
        UInt64 swap_ins;
        UInt64 swap_outs;
        UInt64 failed_swap_outs_space;
    } stats;

    std::ofstream log_file;
    std::string log_file_name;



};

#endif // SWAP_CACHE_H
