#pragma once

#include "simulator.h"
#include "config.hpp"

// NOTE!! buddy_policy.h should be included before reserve_thp.h
// common/system/memory_management/policies
#include "memory_management/policies/buddy_policy.h"           
#include "memory_management/policies/reserve_thp_policy.h"           
#include "memory_management/policies/baseline_allocator_policy.h"

// include/memory_management/physical_memory_allocators/
#include "memory_management/physical_memory_allocators/reserve_thp.h"
#include "memory_management/physical_memory_allocators/baseline_allocator.h"

// PhysicalMemoryAllocator*
// TODO @vlnitu: discuss w/ @kanellok maybe we want to append "include/" to INCLUDE_DIRS in Makefile
#include "memory_management/physical_memory_allocators/physical_memory_allocator.h"

/* TODO @vlnitu: STALE MIGRATE */

// #include "memory_management/physical_memory_allocators/reserve_thp.h"
// #include "eager_paging.h"
// #include "asap.h"
// #include "revelator_allocator.h"
// #include "revelator_open_addressing.h"
// #include "revelator_thp.h"
// #include "spot.h"
// #include "utopia.h"
// #include "mimicos_comm_allocator.h"
// #include "memory_management/physical_memory_allocators/physical_memory_allocator.h"

/* TODO @vlnitu: STALE MIGRATE */

using SniperBaselineAllocator = BaselineAllocator<Sniper::Baseline::MetricsPolicy>;
using SniperTHPAllocator      = ReservationTHPAllocator<Sniper::ReserveTHP::MetricsPolicy>;

class AllocatorFactory
{
public:
    static PhysicalMemoryAllocator *createAllocator(String mimicos_name)
    {

        String allocator_name = Sim()->getCfg()->getString("perf_model/" + mimicos_name + "/memory_allocator_name");
        String allocator_type = Sim()->getCfg()->getString("perf_model/" + mimicos_name + "/memory_allocator_type");
        std::cout << "[MimicOS] [createAllocator] Creating physical memory allocator for " << mimicos_name <<
                     " - allocator_name = " << allocator_name <<
                    "  - allocator_type = " << allocator_type <<  std::endl;

        int memory_size = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/memory_size");
        int kernel_size = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/kernel_size");

        std::cout << "[MimicOS] Kernel size in MB: " << kernel_size << std::endl;
      
        if (allocator_type == "reserve_thp")
        { // Based on FreeBSD's reservation-based THP allocator

            String frag_type = Sim()->getCfg()->getString("perf_model/" + allocator_name + "/frag_type");
            int max_order = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/max_order");
            float threshold_for_promotion = Sim()->getCfg()->getFloat("perf_model/" + allocator_name + "/threshold_for_promotion");
            return new SniperTHPAllocator(allocator_type, memory_size, max_order, kernel_size, frag_type, threshold_for_promotion);
        }
        else if (allocator_type == "baseline")
        {
            String frag_type = Sim()->getCfg()->getString("perf_model/" + allocator_name + "/frag_type");
            int max_order = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/max_order");
            return new SniperBaselineAllocator(allocator_type, memory_size, max_order, kernel_size, frag_type);
        } 
        // else if (allocator_type == "utopia")
        // { // Based on [Kanellopoulos+  MICRO '23]
        //     String frag_type = Sim()->getCfg()->getString("perf_model/" + allocator_name + "/frag_type");
        //     int max_order = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/max_order");
        //     return new Utopia(allocator_name, memory_size, max_order, kernel_size, frag_type);
        // }
        // else if (allocator_type == "revelator")
        // { // Based on [Kanellopoulos+  Under submission]
        //     UInt64 m_memory_size = (UInt64)Sim()->getCfg()->getInt("perf_model/"+allocator_name+"/memory_size");
        //     UInt64 kernel_size = Sim()->getCfg()->getInt("perf_model/"+allocator_name+"/kernel_size");
        //     String frag_type = Sim()->getCfg()->getString("perf_model/" + allocator_name + "/frag_type");
        //     int max_order = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/max_order");
        //     int number_of_hashes = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/number_of_hashes");
        //     double targ_frag=Sim()->getCfg()->getFloat("perf_model/" + allocator_name + "/target_fragmentation");
        //     bool enable_aggressive_swapouts = Sim()->getCfg()->getBool("perf_model/" + allocator_name + "/enable_aggressive_swapouts");
        //     int infrequency_threshold = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/infrequency_threshold");
        //     double hash1_usage_threshold = Sim()->getCfg()->getFloat("perf_model/" + allocator_name + "/hash1_usage_threshold");
        //     return new RevelatorAllocator(allocator_name, max_order, kernel_size, frag_type, number_of_hashes, m_memory_size,targ_frag, enable_aggressive_swapouts, infrequency_threshold, hash1_usage_threshold);
        // }
        // else if (allocator_type== "revelator_thp"){

        //     float threshold_for_promotion = Sim()->getCfg()->getFloat("perf_model/" + allocator_name + "/threshold_for_promotion");
        //     UInt64 m_memory_size = (UInt64)Sim()->getCfg()->getInt("perf_model/"+allocator_name+"/memory_size");
        //     UInt64 kernel_size = Sim()->getCfg()->getInt("perf_model/"+allocator_name+"/kernel_size");
        //     String frag_type = Sim()->getCfg()->getString("perf_model/" + allocator_name + "/frag_type");
        //     int max_order = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/max_order");
        //     int number_of_hashes = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/number_of_hashes");
        //     return new RevelatorTHPAllocator( allocator_name,  memory_size,  max_order, kernel_size, frag_type, number_of_hashes, threshold_for_promotion);
        // }
        // else if(allocator_type == "revelator_open_addressing")
        // {   // Based on [Kanellopoulos+  Under submission]
        //     UInt64 m_memory_size = (UInt64)Sim()->getCfg()->getInt("perf_model/"+allocator_name+"/memory_size");
        //     UInt64 kernel_size = Sim()->getCfg()->getInt("perf_model/"+allocator_name+"/kernel_size");
        //     String frag_type = Sim()->getCfg()->getString("perf_model/" + allocator_name + "/frag_type");
        //     int max_order = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/max_order");
        //     int number_of_hashes = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/number_of_hashes");
        //     double targ_frag=Sim()->getCfg()->getFloat("perf_model/" + allocator_name + "/target_fragmentation");
        //     bool enable_aggressive_swapouts = Sim()->getCfg()->getBool("perf_model/" + allocator_name + "/enable_aggressive_swapouts");
        //     int infrequency_threshold = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/infrequency_threshold");
        //     double hash1_usage_threshold = Sim()->getCfg()->getFloat("perf_model/" + allocator_name + "/hash1_usage_threshold");
        //     int probing_depth = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/probing_depth");
        //     return new RevelatorOpenAddressingAllocator(allocator_name, max_order, kernel_size, probing_depth, frag_type, number_of_hashes, m_memory_size,targ_frag, enable_aggressive_swapouts, infrequency_threshold, hash1_usage_threshold);
        // }
        // else if (allocator_type == "eager_paging")
        // { // Based on Eager Paging [Karakostas+  ISCA 2015]
        //     String frag_type = Sim()->getCfg()->getString("perf_model/"+allocator_name+"/frag_type");
        //     int max_order = Sim()->getCfg()->getInt("perf_model/"+allocator_name+"/max_order");
        //     return new EagerPagingAllocator(allocator_name, memory_size, max_order, kernel_size, frag_type);
        // }
        // else if (allocator_type == "asap")
        // { // Based on ASAP [Margaritov+ MICRO 2019]
        //     String frag_type = Sim()->getCfg()->getString("perf_model/" + allocator_name + "/frag_type");
        //     int max_order = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/max_order");
        //     float threshold_for_promotion = Sim()->getCfg()->getFloat("perf_model/" + allocator_name + "/threshold_for_promotion");
        //     return new ASAPAllocator(allocator_name, memory_size, max_order, kernel_size, frag_type, threshold_for_promotion);
        // }
        // else if (allocator_type == "spot")
        // { // Based on Spot [Alverti+  ISCA 2020]
        //     String frag_type = Sim()->getCfg()->getString("perf_model/" + allocator_name + "/frag_type");
        //     int max_order = Sim()->getCfg()->getInt("perf_model/" + allocator_name + "/max_order");
        //     return new SpotAllocator(allocator_name, memory_size, max_order, kernel_size, frag_type);
        // }
        else
        {
            std::cout << "[Sniper] Allocator not found" << std::endl;
            return nullptr;
        }
    }
};
