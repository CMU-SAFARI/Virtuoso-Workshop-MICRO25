
// #pragma once
// #include "memory_manager.h"
// #include "cache_cntlr.h"
// #include "subsecond_time.h"
// #include "fixed_types.h"
// #include "core.h"
// #include "shmem_perf_model.h"
// #include "pagetable.h"
// #include "tlb_subsystem.h"
// #include "mmu_base.h"
// #include "metadata_table_base.h"

// namespace ParametricDramDirectoryMSI
// {
// 	class TLBHierarchy;

// 	class IOMemoryManagementUnit : public MemoryManagementUnitBase
// 	{

// 	private:
// 		MemoryManager *memory_manager;
// 		TLBHierarchy *tlb_subsystem;

// 		struct
// 		{
// 			UInt64 num_translations;
// 			UInt64 page_faults;
// 			UInt64 page_table_walks;
// 			SubsecondTime total_walk_latency;
// 			SubsecondTime total_translation_latency;
// 			SubsecondTime total_tlb_latency;
// 			SubsecondTime total_fault_latency;
// 			SubsecondTime walker_is_active;
// 			SubsecondTime *tlb_latency_per_level;
// 			UInt64 *tlb_hit_page_sizes; 

// 		} translation_stats;
		
// 	public:
// 		IOMemoryManagementUnit(String name);
// 		~IOMemoryManagementUnit();
// 		void instantiatePageTable();
// 		void instantiateTLBSubsystem();
// 		void registerMMUStats();
// 		pair<SubsecondTime, IntPtr> performAddressTranslation(IntPtr eip, IntPtr address, bool instruction, Core::lock_signal_t lock, bool modeled, bool count);
// 		pair<bool, pair<SubsecondTime, IntPtr>> performAddressTranslationRevelator(IntPtr eip, IntPtr address, bool instruction, Core::lock_signal_t lock, bool modeled, bool count);
// 		void discoverVMAs();
// 	};
// }