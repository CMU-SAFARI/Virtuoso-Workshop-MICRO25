
#pragma once

#include "memory_management/physical_memory_allocators/physical_memory_allocator.h"
#include "memory_management/misc/vma.h"
#include "pagetable.h"
#include "rangetable.h"
#include "subsecond_time.h"
#include "swap_space.h"
#include <unordered_map>

using namespace std;

namespace MimicOS_NS {
    struct Message{
        int argc;
        uint64_t *argv;
    };
}


class MimicOS
{



private:

    PhysicalMemoryAllocator *m_memory_allocator; // This is the physical memory allocator
    bool is_guest;
    String mimicos_name;
    bool vmas_provided;
    

    bool userpace_mimicos_enabled;

    String page_table_type;
    String page_table_name;
    ComponentLatency m_page_fault_latency;

    String range_table_type;
    String range_table_name;

    
    int number_of_page_sizes;
    int *page_size_list;

    // Use an unordered map to store the page table for each application
    std::unordered_map<UInt64, ParametricDramDirectoryMSI::PageTable*> page_tables;
    std::unordered_map<UInt64, ParametricDramDirectoryMSI::RangeTable*> range_tables;

    // Use an unordered map to store the virtual memory areas for each application
    std::unordered_map<UInt64, std::vector<VMA>> vm_areas;

    bool swap_enabled;
    SwapCache* swap_cache;

    bool last_page_fault_caused_swapping;


    std::string log_file_name;
    std::ofstream log_file;

    MimicOS_NS::Message* m_message;

    bool is_page_fault; // @vlnitu: if true, page fault has occured in last PTW; used to replay instruction after CS (context switch)
    int  m_num_requested_frames; // @vlnitu: number of frames requested for page fault handling; used to replay instruction after CS (context switch)
    IntPtr va_triggered_pf; // @vlnitu: the virtual address that caused the page fault

    double m_target_fragmentation;

public:
    MimicOS(bool _is_guest);
    ~MimicOS();


    void createApplication(int app_id);

    String getName() { return mimicos_name; }

    PhysicalMemoryAllocator *getMemoryAllocator() { return m_memory_allocator; }

    ParametricDramDirectoryMSI::PageTable* getPageTable(int app_id) { return page_tables[app_id]; }
    ParametricDramDirectoryMSI::RangeTable* getRangeTable(int app_id) { return range_tables[app_id]; }

    std::vector<VMA>& getVMA(int app_id) { return vm_areas[app_id]; }

    void setAllocatedVMA(int app_id, int vma_index) {
        vm_areas[app_id][vma_index].setAllocated(true);
    }

    void setPhysicalOffset(int app_id, int vma_index, IntPtr offset) {
        vm_areas[app_id][vma_index].setPhysicalOffset(offset);
    }

    IntPtr getPhysicalOffsetSpot(int app_id, IntPtr va) {
        for (auto& vma : vm_areas[app_id]) {
            if (vma.contains(va)) {
                return vma.getPhysicalOffset();
            }
        }
    }

    bool incrementSuccessfulOffsetBasedAllocations(int app_id, IntPtr va) {
        for (auto& vma : vm_areas[app_id]) {
            if (vma.contains(va)) {
                vma.incrementSuccessfulOffsetBasedAllocations();
                return true; // Successfully incremented the count
            }
        }

        return false; // No VMA found for the address
    }


    int getSuccessfulOffsetBasedAllocations(int app_id, IntPtr va) {
        for (auto& vma : vm_areas[app_id]) {
            if (vma.contains(va)) {
                return vma.getSuccessfulOffsetBasedAllocations();
            }
        }

        return -1; // No VMA found for the address
    }

    int getVMAThresholdSpot(int app_id, IntPtr va) {
        for (auto& vma : vm_areas[app_id]) {
            if (vma.contains(va)) {
                return vma.getSuccessfulOffsetBasedAllocations();
            }
        }
        return false; // No VMA found for the address
    }
    
    void setLastPageFaultCausedSwapping(bool caused_swapping) {
        last_page_fault_caused_swapping = caused_swapping;
    }

    UInt64 getAccessesPerVPN(IntPtr vpn, int app_id)
    {
        return getPageTable(app_id)->getAccessesPerVPN(vpn);
    }

    bool swapOutPage(IntPtr vpn, int app_id);
    
    void  deletePageTableEntry(IntPtr vpn, int app_id);
    bool isSwapEnabled() { return swap_enabled; }
    SwapCache* getSwapCache(){  return swap_cache;}

    void setPageTableType(String type) { page_table_type = type; }
    void setPageTableName(String name) { page_table_name = name; }

    String getPageTableType() { return page_table_type; }
    String getPageTableName() { return page_table_name; }

    void setRangeTableType(String type) { range_table_type = type; }
    void setRangeTableName(String name) { range_table_name = name; }

    String getRangeTableType() { return range_table_type; }
    String getRangeTableName() { return range_table_name; }

    int getNumberOfPageSizes() { return number_of_page_sizes; }
    int* getPageSizeList() { return page_size_list; }

    SubsecondTime getPageFaultLatency() { return m_page_fault_latency.getLatency(); }


    static std::unordered_map<std::string, uint64_t> protocol_codes_encode; 
    static std::unordered_map<uint64_t, std::string> protocol_codes_decode; 


    // NOTE: stateful operation: modifies m_message attribute from MimicOS class
    // The message will be processed in virtuos.cc/poll_for_signal() 
    template <typename... Args>
    void buildMessageWithArgs(const std::string& message_type, Args&&... args) {
        const uint64_t protocol_code = protocol_codes_encode[message_type];
        assert(protocol_code != 0 && protocol_code != UINT64_MAX);
        constexpr size_t extra = 1; // argv[0] = protocol_code

        if (m_message == nullptr) {
           m_message = static_cast<MimicOS_NS::Message*>(malloc(sizeof(MimicOS_NS::Message)));
        }

        m_message->argc = extra + sizeof...(args);
        m_message->argv = static_cast<uint64_t*>(malloc(sizeof(uint64_t) * m_message->argc));

        m_message->argv[0] = protocol_code;

        uint64_t args_array[] = { static_cast<uint64_t>(args)... };
        for (size_t i = 0; i < sizeof...(args); ++i) {
            m_message->argv[i + 1] = args_array[i];
        }
    }

    MimicOS_NS::Message* getMessage() {
        return m_message;
    }

    // getIsPageFault() returns true if a page fault has occurred in the last PTW
    bool getIsPageFault() const {
        return is_page_fault;
    }   
    // setIsPageFault() sets the is_page_fault attribute
    void setIsPageFault(bool is_pf) {
        is_page_fault = is_pf;
    }

    // getVaTriggeredPageFault() returns the virtual address that caused the page fault
    IntPtr getVaTriggeredPageFault() const {
        return va_triggered_pf;
    }

    // setVaTriggeredPageFault() sets the va_triggered_pf attribute
    void setVaTriggeredPageFault(IntPtr va) {
        va_triggered_pf = va;
    }
    
    // setNumRequestedFrames() sets the number of requested frames for page fault handling
    void setNumRequestedFrames(int num_frames) {
        m_num_requested_frames = num_frames;
    }

    // getNumRequestedFrames() returns the number of requested frames for page fault handling
    int getNumRequestedFrames() const {
        return m_num_requested_frames;
    }

    // resetPageFaultState() resets the page fault state
    void resetPageFaultState() {
        is_page_fault = false;
        va_triggered_pf = static_cast<IntPtr>(-1);
        m_num_requested_frames = static_cast<int>(-1);
    };

    bool isUserspaceMimicosEnabled() const {
        return userpace_mimicos_enabled;
    }

};