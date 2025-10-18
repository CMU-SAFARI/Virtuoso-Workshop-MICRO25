#pragma once

#include "debug_config.h"
#include "memory_management/physical_memory_allocators/physical_memory_allocator.h"
#include <bitset>
#include <map>
#include <tuple>
#include <vector>
#include <utility>
#include <cassert>

#include "templates_traits_config.h"

// Provides template specialisation for Buddy Type mapping: based on Policy template type used in instantiating ReservationTHPAllocator
#include "memory_management/physical_memory_allocators/buddy_policy_traits.h"
#include "memory_management/physical_memory_allocators/buddy.h"

#include "debug_config.h"

/*
 * ReservationTHPAllocator is a specialized allocator that tries to reserve 2MB pages 
 * whenever possible (i.e., for Transparent Huge Pages or THP). If 2MB reservations fail, 
 * it falls back to a buddy allocator for 4KB pages. This approach allows mixing large pages 
 * (2MB) for contiguous memory when utilization is high enough, and smaller 4KB pages otherwise.
 *
 * Key data structures:
 *   - two_mb_map: Maps a 2MB-region index (region_2MB) to a tuple of:
 *       (1) The starting physical address of that 2MB region,
 *       (2) A bitset<512> representing which 4KB pages within the 2MB region are in use,
 *       (3) A bool flag indicating whether the region has been promoted (fully used as a 2MB page).
 *   - buddy_allocator: A fallback buddy allocator for normal 4KB allocations and for reserving 
 *                      contiguous 2MB blocks when possible.
 *
 * Statistics tracked include:
 *   - four_kb_allocated
 *   - two_mb_reserved
 *   - two_mb_promoted
 *   - two_mb_demoted
 *   - total_allocations
 *   - kernel_pages_used
 *
 * threshold_for_promotion sets the fraction of used 4KB pages in a 2MB region needed to "promote"
 * the entire region to a single 2MB large page mapping.
 */

 using namespace std;
template <typename Policy>
class ReservationTHPAllocator : public PhysicalMemoryAllocator, private Policy
{
    using BuddyPolicy = typename BuddyPolicyFor<Policy>::type;
    using BuddyType =   Buddy<BuddyPolicy>;

private:
            struct Stats {
                UInt64 four_kb_allocated = 0;
                UInt64 two_mb_reserved   = 0;
                UInt64 two_mb_promoted   = 0;
                UInt64 two_mb_demoted    = 0;
                UInt64 total_allocations = 0;
                UInt64 kernel_pages_used = 0;
            } stats;
public:
    Stats& getStats() { return stats; }

    ReservationTHPAllocator(String name,
                            int memory_size,
                            int max_order,
                            int kernel_size,
                            String frag_type,
                            double threshold_for_promotion)
        : PhysicalMemoryAllocator(name, memory_size, kernel_size),
          threshold_for_promotion(threshold_for_promotion)
    {
        Policy::on_init(name, memory_size, kernel_size, threshold_for_promotion, this);

        // PROBLEM: include‑order contract problem: Right now, reserve_thp.h assumes that BuddyPolicyFor<Policy> is fully specialized. If someone forgets to include the side‑specific buddy_policy.h first, compilation fails with a cryptic "incomplete type" error.
        // SOLUTION (self‑documenting and enforceable at compile‑time): define type trait "is_complete" to validate type is complete
        static_assert(is_complete<BuddyPolicyFor<Policy>>::value,
                        "BuddyPolicyFor<Policy> is incomplete. Did you include the correct buddy_policy.h before reserve_thp.h?");

        buddy_allocator = new BuddyType(memory_size, max_order, kernel_size, frag_type);
    }

    ~ReservationTHPAllocator()
    {
        delete buddy_allocator;
    }

    /**
     * allocate(...):
     *   - This is the main entry point for user-level (non-pagetable) allocations.
     *   - If is_pagetable_allocation == true, we pass it on to the buddy allocator 
     *     because page tables are always 4KB in this model.
     *   - Otherwise, we check if we can place the address in a 2MB region via checkFor2MBAllocation(...).
     *     * If that returns a valid address, we use it. Possibly at 21 bits if "promoted," or 12 bits 
     *       if not.
     *     * If 2MB fails, we fallback to the buddy allocator for a 4KB allocation.
     *       - If the buddy also fails, we try demote_page() to free partial 2MB regions, 
     *         and then retry. 
     *
     * Returns:
     *   ( physical_address , page_size_in_bits ) or ( -1 , 12 ) if no memory is available.
     */
    std::pair<UInt64, UInt64> allocate(UInt64 size, UInt64 address = 0, UInt64 core_id = -1, bool is_pagetable_allocation = false) override
    {
        stats.total_allocations++;
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
        this->log("ReservationTHPAllocator::allocate(" + std::to_string(size) +
        ", " + std::to_string(address) +
        ", " + std::to_string(core_id) + ")");
#endif

        // Page table allocations always go to the buddy allocator in 4KB form
        if (is_pagetable_allocation)
        {
            auto page = buddy_allocator->allocate(size, address, core_id);
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
            this->log("Debug: Pagetable allocation, result = " + std::to_string(page));
#endif
            stats.four_kb_allocated++;
            return std::make_pair(page, 12);
        }

        // Attempt to allocate within a 2MB chunk
        auto page2mb = checkFor2MBAllocation(address, core_id);
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
	    this->log("Debug: Checked for 2MB allocation, result = " + std::to_string(page2mb.first));
#endif

        // If we got a valid address, we either just reserved or used an existing 2MB region
        if (page2mb.first != static_cast<UInt64>(-1)) {
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
		this->log("Debug: There is a 2MB allocation (either promoted now or reserved now), returning physical address");
#endif
            if (page2mb.second) { // The page got promoted to 2MB
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
			this->log("Debug: The page just got promoted, returning physical address with 2MB flag");
#endif
			return make_pair(page2mb.first, 21);
            }
            else { // We remain in 4KB mode inside that 2MB region
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
			this->log("Debug: The page is not promoted, returning physical address with 4KB flag");
#endif
			return make_pair(page2mb.first, 12);
            }
        }

		// 2MB allocation did not succeed; fallback to buddy
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
		this->log("Debug: No 2MB allocation, falling back to buddy allocator");
#endif
        auto fallback = buddy_allocator->allocate(size, address, core_id);

		// If buddy works, we get a 4KB allocation
        if (fallback != static_cast<UInt64>(-1))
        {
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
			this->log("Debug: Buddy allocator succeeded, returning physical address with 4KB flag");
#endif
            stats.four_kb_allocated++;
            return std::make_pair(fallback, 12);
        }

        // If buddy fails, try demoting a partially used 2MB region
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
			this->log("Debug: Buddy allocator failed, attempting to demote a page");
#endif
        if (demote_page())
        {
            auto retried = buddy_allocator->allocate(size, address, core_id);
            stats.four_kb_allocated++;
            return std::make_pair(retried, 12);
        }
        else
        {
            assert(false);
            // No pages left to demote => out of memory
            return make_pair((UInt64)-1, 12);
        }
    }

    /*
    * allocate_ranges(...) => Not used in this model. Could be extended if needed.
    */
    std::vector<Range> allocate_ranges(IntPtr start_va, IntPtr end_va, int app_id)
    {
        std::vector<Range> ranges;
        return ranges;
    }


    /*
    * fragment_memory(...) => forcibly fragment memory in the buddy allocator, used for testing.
    */
    void fragment_memory(double target_fragmentation)
    {
        buddy_allocator->fragmentMemory(target_fragmentation);
    }



    UInt64 givePageFast(UInt64 bytes, UInt64 address = 0, UInt64 core_id = -1) override
    {
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
        this->log("ReservationTHPAllocator::givePageFast(" + std::to_string(bytes) + 
        ", "  + std::to_string(address) +
        ", " + std::to_string(core_id) + ")");
#endif
        return buddy_allocator->allocate(bytes, address, core_id);
    }

    /*
    * deallocate(...) => Not implemented. Could free 4KB or 2MB pages from the relevant structures.
    */
    void deallocate(UInt64 region, UInt64 core_id = -1) override
    {
        // TODO: Implement
    }

    IntPtr isLargePageReserved(IntPtr address) { // /* SpecTLB spec engine */ 
        if (two_mb_map.find(address >> 21) != two_mb_map.end())
            return get<0>(two_mb_map[address >> 21]);
        return -1;
    }

protected:
    BuddyType* buddy_allocator;
    std::map<UInt64, std::tuple<UInt64, std::bitset<512>, bool>> two_mb_map;
    double threshold_for_promotion;

    /*
    * demote_page():
    *   - Searches through all allocated 2MB regions in two_mb_map to find the one with the
    *     lowest utilization (least allocated 4KB pages). That region is then "demoted" by 
    *     freeing its unused pages, removing the region from two_mb_map, and letting the 
    *     buddy allocator take back the partially unused space.
    *   - This can free up memory for new allocations. Typically used when no large or small
    *     allocations can succeed, so we demote a region to reclaim partial space.
    *
    * Returns 'true' if a demotion occurred, or 'false' if there are no demotable 2MB regions.
    */
    bool demote_page()
    {
        // We'll collect <2MB_index, utilization> pairs and sort them by ascending utilization
        std::vector<std::pair<UInt64, double>> utilization;

#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
        this->log("Debug: Starting to sort region_2MB based on utilization");
#endif

        for (auto it = two_mb_map.begin(); it != two_mb_map.end(); it++)
        {
            // Skip any regions already "promoted" (fully used or mapped at 2MB)
            if (std::get<2>(it->second))
                continue;

            double util = static_cast<double>(std::get<1>(it->second).count()) / 512;
            utilization.push_back(std::make_pair(it->first, util));
        }

        // Sort by ascending utilization
        std::sort(utilization.begin(), utilization.end(), [](std::pair<UInt64, double> &left, std::pair<UInt64, double> &right)
                { return left.second < right.second; });

#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
        this->log("Debug: Sorted utilization vector");
#endif

        // If there's no 2MB region to demote, return false
        if (utilization.size() == 0)
        {
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
            this->log("Debug: No regions to demote, returning false");
#endif
            return false;
        }

        // Remove the region_2MB from the two_mb_map
        UInt64 region_2MB = utilization[0].first;
        UInt64 region_begin = get<0>(two_mb_map[region_2MB]);
        int region_size = 512; // 512 pages of 4KB each => 2MB total
        int chunk = region_size;

#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
        this->log("Debug: Removing region_2MB with utilization: " + std::to_string(utilization[0].second));
#endif

        for (int j = 0; j < chunk; j++)
        {
            // if the bit is set, then the page is allocated so don't push it
            if (get<1>(two_mb_map[region_2MB])[j])
                continue;

            buddy_allocator->free(region_begin + j, region_begin + (j + 1));
        }

#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
        this->log("Demoted 2MB region with utilization: " +
                  std::to_string(utilization[0].second) +
                  " and freed " +
                  std::to_string(512 - get<1>(two_mb_map[region_2MB]).count()) +
                  " pages");
#endif

        stats.two_mb_demoted++;

        // Remove this region from the map entirely
        two_mb_map.erase(utilization[0].first);
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
        this->log("Debug: Updated stats and erased region_2MB from two_mb_map");
#endif

        return true;
    }

/*
 * checkFor2MBAllocation(...):
 *   - Given a virtual address, determines if the 4KB page can be allocated as part of a 2MB region.
 *   - The region index is region_2MB = address >> 21 (for 2MB alignment).
 *   - If not yet reserved, tries to reserve via buddy_allocator->reserve_2mb_page().
 *   - If successful, records it in two_mb_map[region_2MB] = (region_begin, bitset<512>, not_promoted).
 *   - Then sets the relevant bit (offset_in_2MB) to mark the 4KB page as allocated.
 *   - If the fraction of allocated sub-pages > threshold_for_promotion, mark the region as "promoted."
 *
 * Returns:
 *   ( physical_address_of_4KBpage_within_2MB , bool indicating if just promoted )
 * Or (-1, false) if no reservation could be made.
 */
    std::pair<UInt64, UInt64> checkFor2MBAllocation(UInt64 address, UInt64 core_id)
    {

		UInt64 region_2MB = address >> 21; // Calculate the 2MB region index
        // offset_in_2MB is the 4KB-page index within that chunk
        // We'll recalc it if we do find or create a region
		int offset_in_2MB = (address >> 12) & 0x1FF;

#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
            this->log("Debug: Calculated region_2MB = "     + std::to_string(region_2MB));
            this->log("Debug: Calculated offset_in_2MB = "  + std::to_string(offset_in_2MB));
#endif

        // If we have not yet reserved that 2MB region, try to do so now
		if(two_mb_map.find(region_2MB) == two_mb_map.end()){
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
			this->log("Debug: region_2MB not found in two_mb_map");
#endif

			auto two_mb_reserved_region = buddy_allocator->reserve_2mb_page(address, core_id);
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
			this->log("Debug: Called reserve_2mb_page, result = " +
                      std::to_string(get<0>(two_mb_reserved_region)));
#endif

            // If we couldn't reserve 2MB, return -1
			if(get<0>(two_mb_reserved_region) == static_cast<UInt64>(-1)) {
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
				this->log("Debug: two_mb_reserved_region is nullptr, returning -1");
#endif
                return std::make_pair(-1, false);
			}
			else{
                // We successfully reserved a 2MB chunk, create a new entry in two_mb_map
				two_mb_map[region_2MB] = make_tuple(get<0>(two_mb_reserved_region), bitset<512>(),
                                                    false /* not promoted */);    
				stats.two_mb_reserved++;
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
                std::cout << "debug: reserved 2mb region, updated two_mb_map and stats.two_mb_reserved = " << stats.two_mb_reserved << std::endl;
#endif
			}
		}

        // Retrieve the region info from two_mb_map
		auto& region = two_mb_map[region_2MB];

#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
            this->log("Debug: Retrieved region from two_mb_map");
#endif

            // If region has already been "promoted," we logically shouldn't have a page fault here
            if (std::get<2>(region))
            {
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
                this->log("Debug: Page is already promoted");
#endif
                // If the page was promoted, we wouldn't typically be calling this. So it’s an assert.
                assert(false);
            }
		else { // If the page is not promoted, check if it should be promoted

            // Mark the correct offset within the 2MB region as in use
			auto& bitset = std::get<1>(region);
			int offset_in_2MB = (address >> 12) & 0x1FF;

#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
                this->log("Debug: Page is not promoted, recalculated offset_in_2MB = " + std::to_string(offset_in_2MB));
#endif

                bitset.set(offset_in_2MB); // Mark the page as used

#if DEBUG_RESERVATION_THP >= DEBUG_DETAILED
                this->log("Debug: Marked page as used in bitset: ");
                for (int i = 0; i < 512; i++) {
                    this->log(std::to_string(bitset[i]));
                }
#endif

            // Compute the utilization fraction for this region
            float utilization = static_cast<float>(bitset.count()) / 512;

#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
            this->log("Debug: Calculated utilization = " +
                    std::to_string(utilization) + 
                    " with threshold_for_promotion = " +
                    std::to_string(threshold_for_promotion));
#endif
            // If utilization exceeds the threshold, we "promote" the entire region as a huge page
            bool ready_to_promote = (utilization > threshold_for_promotion);
            if (ready_to_promote && !std::get<2>(region))
            {
                std::get<2>(region) = true;  // Mark as promoted
                stats.two_mb_promoted++;

#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
                this->log("Debug: Promoted page, updated stats.two_mb_promoted = " + 
                        std::to_string(stats.two_mb_promoted));
#endif
                // Return the physical address for the offset_in_2MB, but note that we "just promoted"
                return std::make_pair(std::get<0>(region) + (offset_in_2MB * 4096), true);
            }
            else
            {
                // If not promoted, just return the 4KB offset within the 2MB region
#if DEBUG_RESERVATION_THP >= DEBUG_BASIC
                this->log("Debug: Page is not promoted, returning physical address " + 
                        std::to_string(std::get<0>(region) + (offset_in_2MB * 4096)));
#endif
                return std::make_pair(std::get<0>(region) + (offset_in_2MB * 4096), false);
            }
        }

        // Should not reach here
        return std::make_pair((UInt64)-1, false);
    }

};
