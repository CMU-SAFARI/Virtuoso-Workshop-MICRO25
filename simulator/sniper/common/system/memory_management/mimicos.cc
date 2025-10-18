#include "mimicos.h"
#include "config.hpp"
// #include "page_fault_handler_base.h"
#include "allocator_factory.h"
#include "pagetable_factory.h"
// #include "handler_factory.h"
#include "rangetable_factory.h"
#include "dvfs_manager.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdexcept>
#include "sim_api.h"

#define DEBUG

using namespace std;

// Assuming VMA and other necessary definitions are in included headers
// For example, a simple VMA struct might look like this:

std::unordered_map<std::string, uint64_t> MimicOS::protocol_codes_encode = {
    {"page_fault", 1},
    {"syscall", 2}
};

std::unordered_map<uint64_t, std::string> MimicOS::protocol_codes_decode = {
    {1, "page_fault"},
    {2, "syscall"}
};


MimicOS::MimicOS(bool _is_guest) : m_page_fault_latency(NULL, 0)
{
	std::cout << "Simulator::start() - Entry point for Virtuoso simulation" << " - C++ version: " << __cplusplus << std::endl;


    log_file = std::ofstream();
    // Assuming 'name' is a member of a base class or global. If not, this line will cause an error.
    // Let's assume 'mimicos_name' should be used instead for the log file name.
    log_file_name = std::string(mimicos_name.c_str()) + ".log";
    log_file_name = std::string(Sim()->getConfig()->getOutputDirectory().c_str()) + "/" + log_file_name;
    log_file.open(log_file_name);

#ifdef DEBUG
    if (log_file.is_open()) {
        log_file<< "[DEBUG] Log file opened successfully: " << log_file_name << std::endl;
    } else {
        std::cerr << "[DEBUG] Failed to open log file: " << log_file_name << std::endl;
    }
#endif

#ifdef DEBUG
    log_file<< "[DEBUG] MimicOS::MimicOS Constructor Entry" << std::endl;
#endif

    is_guest = _is_guest;
    if (is_guest)
    {
        mimicos_name = "mimicos_guest";
        log_file<< "[MimicOS] Guest OS is enabled" << std::endl;
    }
    else
    {
        mimicos_name = "mimicos_host";
        log_file<< "[MimicOS] Host OS is enabled" << std::endl;
    }

    log_file<< "[MimicOS] Initializing MimicOS with name: " << mimicos_name << std::endl;

    page_table_type = Sim()->getCfg()->getString("perf_model/" + mimicos_name + "/page_table_type");
    page_table_name = Sim()->getCfg()->getString("perf_model/" + mimicos_name + "/page_table_name");

    range_table_type = Sim()->getCfg()->getString("perf_model/" + mimicos_name + "/range_table_type");
    range_table_name = Sim()->getCfg()->getString("perf_model/" + mimicos_name + "/range_table_name");

    swap_enabled = Sim()->getCfg()->getBool("perf_model/" + mimicos_name + "/swap_enabled");




    userpace_mimicos_enabled = Sim()->getCfg()->getBool("general/enable_userspace_mimicos");
    
#ifdef DEBUG
    log_file<< "[DEBUG] Page Table Type: " << page_table_type << ", Name: " << page_table_name << std::endl;
    log_file<< "[DEBUG] Range Table Type: " << range_table_type << ", Name: " << range_table_name << std::endl;
    log_file<< "[DEBUG] Swap Enabled: " << (swap_enabled ? "true" : "false") << std::endl;
    log_file<< "[DEBUG] Target Fragmentation: " << m_target_fragmentation << std::endl;
#endif

    if (!userpace_mimicos_enabled){
        String allocator_name = Sim()->getCfg()->getString("perf_model/" + mimicos_name + "/memory_allocator_name");
        m_target_fragmentation = Sim()->getCfg()->getFloat("perf_model/" + allocator_name + "/target_fragmentation");
        m_memory_allocator = AllocatorFactory::createAllocator(mimicos_name);
    #ifdef DEBUG
        log_file<< "[DEBUG] Memory Allocator created." << std::endl;
    #endif
        m_memory_allocator->fragment_memory(m_target_fragmentation);
    #ifdef DEBUG
        log_file<< "[DEBUG] Memory fragmented." << std::endl;
    #endif
    }


    // TODO @vlnitu: migrate the other page_fault_handlers.. they are now initiated in core.cc
    // TODO @vlnitu: talk to @kanellok about instantiating ExceptionHandler in Core instead of here
    //     page_fault_handler = HandlerFactory::createHandler(Sim()->getCfg()->getString("perf_model/"+mimicos_name+"/page_fault_handler"), m_memory_allocator, mimicos_name, is_guest);
    // #ifdef DEBUG
    //     log_file<< "[DEBUG] Page Fault Handler of type " << Sim()->getCfg()->getString("perf_model/"+mimicos_name+"/page_fault_handler") << " created." << std::endl;
    // #endif

    m_page_fault_latency = ComponentLatency(Sim()->getDvfsManager()->getGlobalDomain(), Sim()->getCfg()->getInt("perf_model/"+mimicos_name+"/page_fault_latency"));
    last_page_fault_caused_swapping = false;

    number_of_page_sizes = Sim()->getCfg()->getInt("perf_model/" + mimicos_name + "/number_of_page_sizes");
    page_size_list = new int[number_of_page_sizes];

#ifdef DEBUG
    log_file<< "[DEBUG] Number of page sizes: " << number_of_page_sizes << std::endl;
#endif

    for (int i = 0; i < number_of_page_sizes; i++)
    {
        page_size_list[i] = Sim()->getCfg()->getIntArray("perf_model/" + mimicos_name + "/page_size_list", i);
#ifdef DEBUG
        log_file<< "[DEBUG] Page size " << i << ": " << page_size_list[i] << " bytes" << std::endl;
#endif
    }

    if(swap_enabled){
        swap_cache = new SwapCache();
#ifdef DEBUG
        log_file<< "[DEBUG] Swap logic block entered." << std::endl;
#endif
    }
    log_file<< "[MimicOS] Page fault latency is " << m_page_fault_latency.getLatency().getNS() << " ns" << std::endl;
#ifdef DEBUG
    log_file<< "[DEBUG] MimicOS::MimicOS Constructor Exit" << std::endl;
#endif

    is_page_fault = false;
    va_triggered_pf = static_cast<IntPtr>(-1);

}

MimicOS::~MimicOS()
{
#ifdef DEBUG
    log_file<< "[DEBUG] MimicOS::~MimicOS Destructor Entry" << std::endl;
#endif
    delete m_memory_allocator;
    delete[] page_size_list;
    if (log_file.is_open()) {
        log_file.close();
    }
#ifdef DEBUG
    log_file<< "[DEBUG] Memory Allocator and page_size_list deleted. Log file closed." << std::endl;
    log_file<< "[DEBUG] MimicOS::~MimicOS Destructor Exit" << std::endl;
#endif
}

void MimicOS::deletePageTableEntry(IntPtr victim_address, int app_id)
{
#ifdef DEBUG
    log_file<< "[DEBUG] MimicOS::deletePageTableEntry Entry - Address: 0x" << std::hex << victim_address << ", AppID: " << std::dec << app_id << std::endl;
#endif
    if (page_tables.find(app_id) == page_tables.end())
    {
        log_file<< "[MimicOS] Application " << app_id << " does not exist" << std::endl;
        return;
    }

    ParametricDramDirectoryMSI::PageTable *page_table = page_tables[app_id];
#ifdef DEBUG
    log_file<< "[DEBUG] Deleting page 0x" << std::hex << victim_address << " from page table for AppID " << std::dec << app_id << std::endl;
#endif
    page_table->deletePage(victim_address);
#ifdef DEBUG
    log_file<< "[DEBUG] MimicOS::deletePageTableEntry Exit" << std::endl;
#endif
}

void MimicOS::createApplication(int app_id)
{
#ifdef DEBUG
    log_file<< "[DEBUG] MimicOS::createApplication Entry - AppID: " << app_id << std::endl;
#endif

    if (page_tables.find(app_id) != page_tables.end())
    {
        log_file<< "[MimicOS] Application " << app_id << " already exists" << std::endl;
        return;
    }

    log_file<< "[MimicOS] Creating application " << app_id << " with page table type " << page_table_type << " and name " << page_table_name << std::endl;

    // Create a new page table for the application
    ParametricDramDirectoryMSI::PageTable *page_table = ParametricDramDirectoryMSI::PageTableFactory::createPageTable(page_table_type, page_table_name, app_id, is_guest);
    page_tables[app_id] = page_table;
#ifdef DEBUG
    log_file<< "[DEBUG] Page table created for AppID: " << app_id << std::endl;
#endif

    ParametricDramDirectoryMSI::RangeTable *range_table = ParametricDramDirectoryMSI::RangeTableFactory::createRangeTable(range_table_type, range_table_name, app_id);
    range_tables[app_id] = range_table;
#ifdef DEBUG
    log_file<< "[DEBUG] Range table created for AppID: " << app_id << std::endl;
#endif

    log_file<< "[MimicOS] Parsing provided VMAs for application " << app_id << std::endl;

    // Parse the provided VMAs from the file: /path/to/input/trace/trace.vma
    String app_id_str = to_string(app_id).c_str();
    
    if (!Sim()->getCfg()->hasKey("traceinput/thread_" + app_id_str))
    {
        log_file<< "[MimicOS] No VMA file provided for application " << app_id << std::endl;
        return;
    }

    String trace_file = Sim()->getCfg()->getString("traceinput/thread_" + app_id_str);
    
    std::ifstream trace((trace_file + ".vma").c_str());

    if (!trace.is_open())
    {
        std::cerr << "[MimicOS] Error opening file " << trace_file << ".vma" << std::endl;
        return;
    }
#ifdef DEBUG
    log_file<< "[DEBUG] Opened VMA file: " << trace_file << ".vma" << std::endl;
#endif

    std::vector<VMA> vmas;
    std::string line;
    while (std::getline(trace, line))
    {
        std::stringstream ss(line);
        std::string startStr, endStr;

        if (std::getline(ss, startStr, '-') && std::getline(ss, endStr))
        {
            try
            {
                IntPtr start = std::stoull(startStr, nullptr, 16);
                IntPtr end = std::stoull(endStr, nullptr, 16);

#ifdef DEBUG
                log_file<< "[DEBUG] Read VMA line: " << line << " -> Parsed Start: 0x" << std::hex << start << ", End: 0x" << end << std::dec << std::endl;
#endif
                VMA vma(start, end);
                vmas.push_back(vma);
            }
            catch (const std::invalid_argument &e)
            {
                std::cerr << "Error: Invalid VMA format in line: " << line << std::endl;
            }
            catch (const std::out_of_range &e)
            {
                std::cerr << "Error: VMA out of range in line: " << line << std::endl;
            }
        }
        else
        {
            std::cerr << "Error: Invalid line format: " << line << std::endl;
        }
    }

    vm_areas[app_id] = vmas;
    log_file<< "[MimicOS] VMAs for application " << app_id << " have been parsed" << std::endl;

// Print the VMAs for the application

#ifdef DEBUG
    log_file<< "[DEBUG] MimicOS::createApplication Exit" << std::endl;
#endif
    return;
}

bool MimicOS::swapOutPage(IntPtr vpn, int app_id)
{   
    bool is_memory_full = false;


    if (swap_enabled) {
#ifdef DEBUG
    log_file << "Swapping out page: " << vpn << " for app_id: " << app_id << std::endl;
#endif
        return swap_cache->swapOut(vpn, app_id, is_memory_full);
    }

    return false;
}