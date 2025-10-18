#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>	  /* For O_* constants */
#include <sys/stat.h> /* For mode constants */
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <vector>
#include "fixed_types.h"
#include "reserve_thp_policy.h"
// #include "buddy.h"
#include "reserve_thp.h"
#include "swap_space.h"
#include "nn.h"

#include <string>
#define START_SPECIAL_AREA 0
#define END_SPECIAL_AREA 123456789
#if defined(__i386)
#define MAGIC_REG_A "eax"
#define MAGIC_REG_B "edx" // Required for -fPIC support
#define MAGIC_REG_C "ecx"
#else
#define MAGIC_REG_A "rax"
#define MAGIC_REG_B "rbx"
#define MAGIC_REG_C "rcx"
#endif
#define SimMagic0(cmd)                                     \
	({                                                     \
		unsigned long _cmd = (cmd), _res;                  \
		__asm__ __volatile__("mov %1, %%" MAGIC_REG_A "\n" \
							 "\txchg %%bx, %%bx\n"         \
							 : "=a"(_res) /* output    */  \
							 : "g"(_cmd)  /* input     */  \
		);								  /* clobbered */  \
		_res;                                              \
	})
#define NUMBER_OF_DUMMY_INSTR 10

/**
 * @struct PTEntry
 * @brief Simple page-table entry used by the trace generator / replacement simulation.
 *
 * Encapsulates the minimal state required for page-replacement bookkeeping:
 * a "referenced" (second-chance) bit and the associated physical address.
 *
 * Fields
 * - referenced: A boolean flag indicating whether the page has been recently accessed.
 *               This flag is used by replacement algorithms (e.g., second-chance) to
 *               decide whether to give the page another chance before eviction.
 * - physical_address: The 64-bit physical address (or frame identifier) mapped to this
 *                     page-table entry.
 */
struct PTEFields {
	uint64_t ppn;
	uint64_t page_size;
	bool referenced;
	uint64_t age;
	uint64_t vma_type;
	uint64_t page_type;

	void secondChance() {
		referenced = false;
	}

	PTEFields() : referenced(false), ppn(0), page_size(0), age(0), vma_type(0), page_type(0) {}
	PTEFields(bool ref, uint64_t ppn, uint64_t page_size) : ppn(ppn), page_size(page_size), referenced(ref), age(0), vma_type(0), page_type(0) {}
};

using PT = std::map<uint64_t, PTEFields>;

/**
 * Select a victim page from the page table using a clock / second-chance style scan.
 *
 * The function performs repeated scans of the ordered pagetable starting from the position
 * indicated by 'hand' and collects up to k (k == 2) candidate entries that are not
 * currently referenced. During the scan, any entry that is referenced will have its
 * PTEntry::secondChance() method invoked and the scan continues. The 'hand' iterator is
 * advanced as entries are inspected and wrapped to pagetable.begin() when the end is reached.
 *
 * Parameters:
 * - pagetable: reference to the map of page-key -> PTEntry to search.
 * - hand: reference to a pagetable iterator indicating the current clock hand position.
 *         This iterator will be advanced (and possibly wrapped) by this function.
 * - chosen_index: index into the list of collected candidates to select as the victim.
 *
 * Returns:
 * - The map key (as an IntPtr) of the selected victim entry corresponding to
 *   candidates[chosen_index].
 */
std::vector<PT::iterator> get_victim_candidates(PT &pagetable, PT::iterator &hand) {
	int k = 2;
	std::vector<PT::iterator> candidates;

	while (true) {
		candidates.clear();
		int count = 0;
		while (count < k && hand != pagetable.end()) {
			// if (!hand->second.referenced) {
			// 	candidates.push_back(hand);
			// } else {
			// 	hand->second.secondChance();
			// }
			candidates.push_back(hand);
			hand++;
			count++;
		}
		if (hand == pagetable.end()) {
			hand = pagetable.begin();
		}
		if (!candidates.empty()) {
			break;
		}
	}

	return candidates;
}

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		std::cerr << "Usage: " << argv[0] << " <input_app_trace> <r2_trace> <mqsim_trace>" << std::endl;
		return 1;
	}
	// Create a ReservationTHPAllocator
	ReservationTHPAllocator<Ramulator::ReserveTHP::MetricsPolicy> *allocator = new ReservationTHPAllocator<Ramulator::ReserveTHP::MetricsPolicy>("ReservationTHPAllocator", 1024 + 2, 0, 1024, "contiguity", 1.0);

	// @hsongaraVTW25: Create a SwapCache
	SwapCache* swap_cache = new SwapCache(8192);

	// @hsongaraVTW25: Maintain a Page table
	PT pagetable;
	PT::iterator hand = pagetable.begin();

	UInt64 ppn;
	UInt64 page_size;
	UInt64 physical_address;

	// @hsongaraVTW25: A 'fancy' eviction policy
	NN* nn_evictor = new NN();
	IntPtr victim_vpn = -1;

	// @hsongaraVTW25: Open the input trace file and the output trace file
	std::ifstream input_app_trace(argv[1]);
	
	// Open two output files: one for r2 trace and one for mqsim trace
	std::ofstream r2_trace_out(argv[2]);
	std::ofstream mqsim_trace_out(argv[3]);

	std::string line;

	// Process the input trace file line by line
	if (input_app_trace.is_open())
	{
		int line_number = 0;
		while (getline(input_app_trace, line))
		{
			UInt64 address;
			UInt64 bubbles;
			sscanf(line.c_str(), "%llu %llu", &bubbles, &address);

			uint64_t vpn = address >> 12;
			victim_vpn = -1;
			
			// @hsongaraVTEW25: Check if the page is in the page table
			if (pagetable.find(vpn) == pagetable.end()) {

				// @hsongaraVTW25: Page fault handling -> we enter the magic region
				SimMagic0(38);

				// @hsongaraVTW25: Try to allocate a new page
				std::pair<UInt64, UInt64> result = allocator->allocate(4096, address, 0);

				// @hsongaraVTW25: If allocation failed, we need to evict a page
				if (result.first == (UInt64)-1) {
					// @hsongaraVTW25: Find a victim address to swap out
					std::vector<PT::iterator> victim_candidates = get_victim_candidates(pagetable, hand);
					PT::iterator victim;

					// @hsongaraVTW25: If we have 2 eviction candidates use the predictor, else pick the first one
					if (victim_candidates.size() == 2) {
						Matrix inputs = {
							{static_cast<double>(victim_candidates[0]->first),
								static_cast<double>(victim_candidates[0]->second.ppn),
								static_cast<double>(victim_candidates[0]->second.page_size),
								static_cast<double>(victim_candidates[0]->second.page_type),
								static_cast<double>(victim_candidates[0]->second.age), 
								static_cast<double>(victim_candidates[0]->second.vma_type)},
							{static_cast<double>(victim_candidates[1]->first),
								static_cast<double>(victim_candidates[1]->second.ppn),
								static_cast<double>(victim_candidates[1]->second.page_size),
								static_cast<double>(victim_candidates[1]->second.page_type),
								static_cast<double>(victim_candidates[1]->second.age), 
								static_cast<double>(victim_candidates[1]->second.vma_type)}
						};
						int victim_idx = nn_evictor->predict(inputs);
						victim = victim_candidates[victim_idx];
					} else {
						victim = victim_candidates[0];
					}

					// @hsongaraVTW25: Update hand
					hand = victim;
					if (hand == pagetable.end())
						hand = pagetable.begin();
					else
						hand++;

					// @hsongaraVTW25: Get victim VPN
					victim_vpn = victim->first;

					// @hsongaraVTW25: Swap out the victim address
					std::tuple<bool, IntPtr> swap_result = swap_cache->swapOut(victim_vpn, 0, false);
					if (!std::get<0>(swap_result)) {
						assert(false);
					}

					// @hsongaraVTW25: Write to the mqsim trace the swap out event
					mqsim_trace_out << "0 0 " << std::get<1>(swap_result) * 8 << " 8 0" << std::endl;

					// @hsongaraVTW25: Allocate the freed page to the requesting address
					ppn = pagetable[victim_vpn].ppn;
					page_size = pagetable[victim_vpn].page_size;
					pagetable.erase(victim_vpn);
					pagetable[vpn] = PTEFields{false, ppn, page_size};

					physical_address = (ppn << 12) + (address & 0xFFF);

					// @hsongaraVTW25: Check if this is a part of the swap cache
					bool in_swap = swap_cache->lookup(vpn, 0);
					IntPtr swap_location;
					if (in_swap) {
						swap_location = swap_cache->swapIn(vpn, 0);

						// @hsongaraVTW25: Write to the mqsim trace the swap in event
						mqsim_trace_out << "0 0 " << swap_location * 8 << " 8 1" << std::endl;
					}

				} else {
					// @hsongaraVTW25: Update pagetable
					pagetable[vpn] = PTEFields{false, result.first, result.second};

					physical_address = (result.first << 12) + (address & 0xFFF);
				}
				
				// @hsongaraVTW25: Page fault handling -> we exit the magic region
				SimMagic0(0);

				// @hsongaraVTW25: Copy everything in trace.out into final_app_trace.out
				std::ifstream trace_infile("trace.out");
				
				std::string trace_line;
				if (trace_infile.is_open() && r2_trace_out.is_open()) {
					while (getline(trace_infile, trace_line)) {
						r2_trace_out << trace_line << std::endl;
					}
					trace_infile.close();
				}
				
				// @hsongaraVTW25:Copy the line into final_app_trace.out, except replace the address with physical_address
				r2_trace_out << bubbles << " " << physical_address << std::endl;

				// @hsongaraVTW25: Empty the trace.out for the next line of the input trace
				std::ofstream trace_outfile("trace.out", std::ios::trunc);
				if (trace_outfile.is_open()) {
					trace_outfile.close();
				}
			}
			else {
				// @hsongaraVTW25: If we find the entry, we can directly write to the final_app_trace.out
				pagetable[vpn].referenced = true;
				physical_address = (pagetable[vpn].ppn << 12) + (address & 0xFFF);
				r2_trace_out << bubbles << " " << physical_address << std::endl;
			}


			line_number++;
		}

		// Close the file stream once all lines have been
		// read.
		input_app_trace.close();
		r2_trace_out.close();
		mqsim_trace_out.close();
	}
	else
	{
		// Print an error message to the standard error
		// stream if the file cannot be opened.
		std::cerr << "Unable to open file!" << std::endl;
	}
}