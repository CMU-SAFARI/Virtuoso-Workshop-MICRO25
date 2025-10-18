
#include "mmu_midgard.h"
#include "memory_manager.h"
#include "cache_cntlr.h"
#include "subsecond_time.h"
#include "fixed_types.h"
#include "pagetable_factory.h"
#include "core.h"
#include "thread.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include "rangelb.h"
#include "rangetable_btree.h"
#include "rangetable_factory.h"
#include "simulator.h"
#include "mimicos.h"
//#define DEBUG_MMU

using namespace std;

namespace ParametricDramDirectoryMSI
{

	MemoryManagementUnitMidgard::MemoryManagementUnitMidgard(Core *_core, MemoryManager *_memory_manager, ShmemPerfModel *_shmem_perf_model, String _name, MemoryManagementUnitBase* _nested_mmu)
	: MemoryManagementUnitBase(_core, _memory_manager, _shmem_perf_model, _name, _nested_mmu),
		core(_core),
		memory_manager(_memory_manager),
		shmem_perf_model(_shmem_perf_model)
	{
		log_file = std::ofstream();
		log_file_name = "mmu_midgard.log." + std::to_string(core->getId());
		log_file_name = std::string(Sim()->getConfig()->getOutputDirectory().c_str()) + "/" + log_file_name;
		log_file.open(log_file_name.c_str());
		// In Midgard, all these components are used in the frontend
		instantiateFrontendVLBs();
		// We make the assumption that each MMU has its own designated page table walkers 
		instantiateBackendPageTableWalker();
		instantiateBackendTLBSubsystem();
		registerMMUStats();
	}

	MemoryManagementUnitMidgard::~MemoryManagementUnitMidgard()
	{
	}
	
	
	void MemoryManagementUnitMidgard::instantiateFrontendVLBs()
	{
		String type = Sim()->getCfg()->getString("perf_model/"+name+"/frontend_l1_vlb/type");
		int size = Sim()->getCfg()->getInt("perf_model/"+name+"/frontend_l1_vlb/size");
		int assoc = Sim()->getCfg()->getInt("perf_model/"+name+"/frontend_l1_vlb/assoc");
		int page_sizes = Sim()->getCfg()->getInt("perf_model/"+name+"/frontend_l1_vlb/page_size");
	 	int *page_size_list = (int *)malloc(sizeof(int) * (page_sizes));
		bool allocate_on_miss = Sim()->getCfg()->getBool("perf_model/"+name+"/frontend_l1_vlb/allocate_on_miss");
		ComponentLatency latency = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/"+name+"/frontend_l1_vlb/access_latency"));

		for (int i = 0; i < page_sizes; i++)	
			page_size_list[i] = Sim()->getCfg()->getIntArray("perf_model/"+name+"/frontend_l1_vlb/page_size_list", i);

		frontend_l1vlb = new TLB("frontendl1vlb", "perf_model/"+name+"/frontend_l1_vlb", core->getId(), latency, size, assoc, page_size_list, page_sizes, type, allocate_on_miss);
		std::cout << "[MMU] Frontend L1 VLB instantiated with type: " << type << ", size: " << size << ", associativity: " << assoc << ", page sizes: " << page_sizes << std::endl;

						
        int num_sets = Sim()->getCfg()->getInt("perf_model/" + name + "/frontend_l2_vlb/num_sets");
		latency = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/" + name + "/frontend_l2_vlb/latency"));
		frontend_l2vlb = new RLB(core, "frontend_l2_vlb", latency, num_sets);
		std::cout << "[MMU] Frontend L2 VLB instantiated with num_sets: " << num_sets << ", latency: " << latency.getLatency().getNS() << " ns" << std::endl;

	}



	void MemoryManagementUnitMidgard::instantiateBackendPageTableWalker()
	{
		String mimicos_name = Sim()->getMimicOS()->getName();
		String page_table_type = Sim()->getCfg()->getString("perf_model/" + mimicos_name + "/page_table_type");
		String page_table_name = Sim()->getCfg()->getString("perf_model/" + mimicos_name + "/page_table_name");

		if (page_table_type == "radix")
		{
			int levels = Sim()->getCfg()->getInt("perf_model/" + mimicos_name + "/" + page_table_name + "/levels");
			m_pwc_enabled = Sim()->getCfg()->getBool("perf_model/" + name + "/pwc/enabled");

			if (m_pwc_enabled)
			{

				std::cout << "[MMU] Page walk caches are enabled" << std::endl;

				UInt32 *entries = (UInt32 *)malloc(sizeof(UInt64) * (levels - 1));
				UInt32 *associativities = (UInt32 *)malloc(sizeof(UInt64) * (levels - 1));
				for (int i = 0; i < levels - 1; i++)
				{
					entries[i] = Sim()->getCfg()->getIntArray("perf_model/" + name + "/pwc/entries", i);
					associativities[i] = Sim()->getCfg()->getIntArray("perf_model/" + name + "/pwc/associativity", i);
				}

				ComponentLatency pwc_access_latency = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/" + name + "/pwc/access_penalty"));
				ComponentLatency pwc_miss_latency = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/" + name + "/pwc/miss_penalty"));
				pwc = new PWC("pwc", "perf_model/" + name + "/pwc", core->getId(), associativities, entries, levels - 1, pwc_access_latency, pwc_miss_latency, false);
			}
		}

		pt_walkers = new MSHR(Sim()->getCfg()->getInt("perf_model/" + name + "/page_table_walkers"));
	}

	void MemoryManagementUnitMidgard::instantiateBackendTLBSubsystem()
	{
		tlb_subsystem = new TLBHierarchy(name, core, memory_manager, shmem_perf_model);
	}
	void MemoryManagementUnitMidgard::registerMMUStats()
	{
		bzero(&translation_stats, sizeof(translation_stats));

		// Statistics for the whole MMU

		registerStatsMetric(name, core->getId(), "page_faults", &translation_stats.page_faults);
		registerStatsMetric(name, core->getId(), "total_table_walk_latency", &translation_stats.total_walk_latency);
		registerStatsMetric(name, core->getId(), "total_fault_latency", &translation_stats.total_fault_latency);
		registerStatsMetric(name, core->getId(), "total_tlb_latency", &translation_stats.total_tlb_latency);
		registerStatsMetric(name, core->getId(), "num_frontend_translations", &translation_stats.num_frontend_translations);
		registerStatsMetric(name, core->getId(), "num_backend_translations", &translation_stats.num_backend_translations);
		registerStatsMetric(name, core->getId(), "frontend_latency", &translation_stats.frontend_latency);
		registerStatsMetric(name, core->getId(), "backend_latency", &translation_stats.backend_latency);
		registerStatsMetric(name, core->getId(), "frontend_memory_accesses", &translation_stats.num_frontend_memory_accesses);

		// Statistics for TLB subsystem
		translation_stats.tlb_latency_per_level = new SubsecondTime[tlb_subsystem->getTLBSubsystem().size()];
		std::cout << "TLB Subsystem Size: " << tlb_subsystem->getTLBSubsystem().size() << std::endl;
		for (UInt32 i = 0; i < tlb_subsystem->getTLBSubsystem().size(); i++)
			registerStatsMetric(name, core->getId(), "tlb_latency_" + itostr(i), &translation_stats.tlb_latency_per_level[i]);
	}
	
	IntPtr MemoryManagementUnitMidgard::performAddressTranslationFrontend(IntPtr eip, IntPtr address, bool instruction, Core::lock_signal_t lock, bool modeled, bool count)
	{

		SubsecondTime time = shmem_perf_model->getElapsedTime(ShmemPerfModel::_USER_THREAD);

		SubsecondTime charged_range_walk_latency = SubsecondTime::Zero();
		CacheBlockInfo* hit_l1vlb = frontend_l1vlb->lookup(address, time, count, lock, eip, modeled, count, NULL);

		VMA found_vma = findVMA(address); // This is to ensure that the VMA is created if it does not exist
	


#ifdef DEBUG_MMU
		log_file << "[MMU] ---- Starting address translation for virtual address: " << address << " at time " << shmem_perf_model->getElapsedTime(ShmemPerfModel::_USER_THREAD) << std::endl;
		log_file << "[MMU] VMA found for address: " << address << " with start: " << found_vma.getBase() << " and end: " << found_vma.getEnd() << std::endl;
		log_file << "[MMU] L1 VLB Hit: " << (hit_l1vlb != NULL) << " for address: " << address << std::endl;
#endif
		RangeTable *range_table = Sim()->getMimicOS()->getRangeTable(core->getThread()->getAppId());

		if (hit_l1vlb != NULL)
		{

#ifdef DEBUG_MMU
			log_file << "Hit in L1 VLB for address: " << address << std::endl;
#endif
			// We found the entry in the L1 VLB
			IntPtr lpn_result = hit_l1vlb->getPPN();
			IntPtr page_size = hit_l1vlb->getPageSize();
			IntPtr lp_offset = address % (1 << page_size);
			IntPtr vpn = address >> page_size;

#ifdef DEBUG_MMU
			log_file << "L1 VLB Hit: Address: " << address << " VPN: " << vpn << " PPN: " << lpn_result << " Offset: " << lp_offset << std::endl;
#endif	
			IntPtr logical_address = lpn_result* (1 << page_size) + lp_offset;
			
		}
		else{

			// We did not find the entry in the L1 VLB, we need to check the L2 VLB	
			auto hit_l2vlb = frontend_l2vlb->access(Core::mem_op_t::READ, address, count);

			if (hit_l2vlb.first) // We found the entry in the L2 VLB
			{
#ifdef DEBUG_MMU
				log_file << "Hit in L2 VLB for address: " << address << std::endl;
#endif

			}
			else{
#ifdef DEBUG_MMU
				log_file << "Miss in L2 VLB for address: " << address << std::endl;
#endif
				RangeTable *range_table = Sim()->getMimicOS()->getRangeTable(core->getThread()->getAppId());
				auto result = range_table->lookup(address);
				
			if (get<0>(result) != NULL) // TreeNode* is not NULL
			{
				// We found the key inside the range table
#ifdef DEBUG_MMU
				log_file << "Key found for address: " << address << " in the range table" << std::endl;
#endif
				Range range;
				range.vpn = get<0>(result)->keys[get<1>(result)].first;
				range.bounds = get<0>(result)->keys[get<1>(result)].second;
				range.offset = 0;

#ifdef DEBUG_MMU
				log_file << "VPN: " << range.vpn << " Bounds: " << range.bounds << " Offset: " << range.offset << std::endl;
#endif

#ifdef DEBUG_MMU
				log_file << "Range walk accessed addresses: ";
				for (auto &address : get<2>(result))
				{
					log_file << address << " ";
				}
				log_file << std::endl;
#endif
				SubsecondTime t_start_range = shmem_perf_model->getElapsedTime(ShmemPerfModel::_USER_THREAD);

				// Insert the entry in the RLB
				for (auto &address : get<2>(result))
				{
					
					translationPacket packet;
					packet.address = address;
					packet.eip = eip;
					packet.instruction = false;
					packet.lock_signal = lock;
					packet.modeled = modeled;
					packet.count = count;
					packet.type = CacheBlockInfo::block_type_t::PAGE_TABLE;
#ifdef DEBUG_MMU
					log_file << "Accessing address: " << address << " for range walk" << std::endl;
#endif
					// We access the L1 VLB for the range walk
					charged_range_walk_latency += accessCache(packet, t_start_range, false);
#ifdef DEBUG_MMU
					log_file << "Current range walk latency: " << charged_range_walk_latency.getNS() << " ns" << std::endl;
#endif
				}
#ifdef DEBUG_MMU
				log_file << "Total Range walk latency: " << charged_range_walk_latency.getNS() << " ns" << std::endl;
#endif
				frontend_l2vlb->insert_entry(range);
			}


			}
			

			if (frontend_l1vlb->getAllocateOnMiss()){
				int page_size = Sim()->getCfg()->getInt("perf_model/" + name + "/frontend_l1_vlb/page_size_list");
				IntPtr ppn_result = address >> page_size; // This is the logical address  - we do not need to differentiate between logical and virtual addresses in this case
				frontend_l1vlb->allocate(address, time, count, lock, page_size, ppn_result);
			}
		}

			
	
		return address; // We return the address as it is, since we found it in the L1 VLB
	}
	IntPtr MemoryManagementUnitMidgard::performAddressTranslationBackend(IntPtr eip, IntPtr address, bool instruction, Core::lock_signal_t lock, bool modeled, bool count)
	{
		return address;
	}

	VMA MemoryManagementUnitMidgard::findVMA(IntPtr address)
	{
		int app_id = core->getThread()->getAppId();
		std::vector<VMA> vma_list = Sim()->getMimicOS()->getVMA(app_id);

		for (UInt32 i = 0; i < vma_list.size(); i++)
		{
			if (address >= vma_list[i].getBase() && address < vma_list[i].getEnd())
			{
#ifdef DEBUG_MMU
				log_file << "VMA found for address: " << address << " in VMA: " << vma_list[i].getBase() << " - " << vma_list[i].getEnd() << std::endl;
#endif
				return vma_list[i];
			}
		}
		assert(false);
		return VMA(-1, -1);
	}

	PTWResult MemoryManagementUnitMidgard::filterPTWResult(PTWResult ptw_result, PageTable *page_table, bool count)
	{
		accessedAddresses ptw_accesses;

		if (m_pwc_enabled)
		{
			accessedAddresses original_ptw_accesses = get<1>(ptw_result);
			// We need to filter based on the page walk caches
			for (UInt32 i = 0; i < get<1>(ptw_result).size(); i++)
			{
				bool pwc_hit = false;

					// We need to check if the entry is in the PWC
					// If it is, we need to remove it from the PTW result
					// If it is not, we need to add it to the PTW result
					// Only check page walk caches if the level is not the first one

					int level = get<1>(original_ptw_accesses[i]);

					IntPtr pwc_address = get<2>(original_ptw_accesses[i]);
#ifdef DEBUG_MMU
					log_file << "[MMU] Checking PWC for address: " << pwc_address << " at level: " << level << std::endl;
#endif
					if (level < max_pwc_level){
						pwc_hit = pwc->lookup(pwc_address, SubsecondTime::Zero(), true, level, count);
					}
					

#ifdef DEBUG_MMU
					log_file << "[MMU] PWC HIT: " << pwc_hit << " level: " << level << std::endl;
#endif
					// If the entry is not in the cache, we need to access the memory

					if (!pwc_hit)
					{
						// The entry is stored in: current_frame->emulated_ppn * 4096 which is the physical address of the frame
						// The offset is the index of the entry in the frame
						// The size of the entry is 8 bytes
						// The physical address of the entry is: current_frame->emulated_ppn * 4096 + offset*8
						ptw_accesses.push_back(get<1>(ptw_result)[i]);
					}
			}
		}

		return PTWResult(get<0>(ptw_result), ptw_accesses, get<2>(ptw_result), get<3>(ptw_result), get<4>(ptw_result));
	}

	void MemoryManagementUnitMidgard::discoverVMAs()
	{

		return;
	}

}
