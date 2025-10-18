#pragma once

#include "mmu.h"
#include "mmu_base.h"
#include "mmu_pomtlb.h"
#include "mmu_range.h"
#include "mmu_virt.h"
#include "mmu_dmt.h"
#include "mmu_hw_faults.h"
#include "config.hpp"


namespace ParametricDramDirectoryMSI
{
    class MMUFactory
    {
    public:
        static MemoryManagementUnitBase *createMemoryManagementUnit(String type, Core *core, MemoryManager *memory_manager, ShmemPerfModel *shmem_perf_model, String name, MemoryManagementUnitBase *nested_mmu = nullptr)
        {

			if (type == "default") // Baseline MMU design
			{
				return new MemoryManagementUnit(core, memory_manager, shmem_perf_model, name, nested_mmu);
			}
			else if (type == "pomtlb") // Zu et al. ISCA 2017
			{
				return new MemoryManagementUnitPOMTLB(core, memory_manager, shmem_perf_model, name, nested_mmu);
			}
			else if (type == "range") // Karakostas et al. "" ISCA 2015
			{
				return new RangeMMU(core, memory_manager, shmem_perf_model, name, nested_mmu);
			}
			else if (type == "virt") // Baseline virt MMU design
			{
				return new MemoryManagementUnitVirt(core, memory_manager, shmem_perf_model, name, nested_mmu);
			}
			else if (type == "dmt") // Baseline virt MMU design
			{
				return new MemoryManagementUnitDMT(core, memory_manager, shmem_perf_model, name, nested_mmu);
			}
			else if (type == "hw_fault")	// Hardware Fault Handling MMU design
			{
				return new MemoryManagementUnitHWFault(core, memory_manager, shmem_perf_model, name, nested_mmu);
			}
			else
			{
				std::cerr << "Invalid MMU type: " << type << std::endl;
				abort();
			}
		}
	};
}