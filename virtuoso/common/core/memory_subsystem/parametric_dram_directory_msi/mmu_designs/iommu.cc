
// #include <iostream>
// #include <queue>
// #include "mmu_base.h"
// #include "iommu.h"
// #include "tlb_subsystem.h"

// using namespace std;
// using namespace ParametricDramDirectoryMSI;


// IOMemoryManagementUnit::IOMemoryManagementUnit(String name) : MemoryManagementUnitBase(NULL, NULL, NULL, name, NULL)
// {
//     // Instantiate the page table
//     instantiatePageTable();
//     // Instantiate the TLB subsystem
//     instantiateTLBSubsystem();
//     // Register MMU stats
// }

// void IOMemoryManagementUnit::registerMMUStats()
// {
//     // Register the MMU stats
// }


// IOMemoryManagementUnit::~IOMemoryManagementUnit()
// {
//     delete tlb_subsystem;
//     delete page_table;
// }

// void IOMemoryManagementUnit::instantiatePageTable()
// {

// }


// void IOMemoryManagementUnit::instantiateTLBSubsystem()
// {
//    tlb_subsystem = new TLBHierarchy(name, NULL, NULL, NULL);

// }


// pair<SubsecondTime, IntPtr> IOMemoryManagementUnit::performAddressTranslation(IntPtr eip, IntPtr address, bool instruction, Core::lock_signal_t lock, bool modeled, bool count)
// {
//     // Perform the address translation
   
//     std::cout << "[IOMMU] Performing address translation for IO request: " << address << std::endl;

    

//     // Return the translated address
//     return std::make_pair(SubsecondTime::Zero(), address);

// }

// void IOMemoryManagementUnit::discoverVMAs()
// {
//     // No need to discover VMAs for IOMMU
// }


// pair<bool, pair<SubsecondTime, IntPtr>> IOMemoryManagementUnit::performAddressTranslationRevelator(IntPtr eip, IntPtr address, bool instruction, Core::lock_signal_t lock, bool modeled, bool count)
// {
// 	std::cout << "Relevator not implemented for this MMU" << std::endl;
// 	exit(1);
// 	return std::make_pair(false, std::make_pair(SubsecondTime::Zero(), 0));

// }

