#pragma once

#include <fstream>
#include <iostream>

#include "buddy_policy_traits.h"

#include "reserve_thp_policy.h"
#include "baseline_allocator_policy.h"

namespace Ramulator
{
    namespace Buddy
    {
        struct MetricsPolicy
        {
            mutable std::ofstream log_file;

            ~MetricsPolicy()
            {
                if (log_file.is_open())
                    log_file.close();
            }

            void on_init(int mem_size, int max_order, int kernel_size)
            {
                // std::string log_file_name = std::string(Sim()->getConfig()->getOutputDirectory().c_str()) + "/buddy.log";
                // log_file.open(log_file_name);
                // if (!log_file.is_open())
                // {
                //     throw std::runtime_error("[BUDDY] Failed to open log file");
                // }

                // log_file << "[Buddy] Init: mem_size " << mem_size << "KB" << " max_order = " << max_order << " kernel size = " << kernel_size << "KB" << std::endl;
            }

            void on_out_of_memory(UInt64 bytes, UInt64 addr, UInt64 core)
            {
                // log_file << "[Buddy] Out of memory for " << bytes << " bytes" << " addr = " << addr << " core = " << core << std::endl;
            }

            void on_fragmentation_done()
            {
                // log_file << "[Buddy] Fragmentation complete" << std::endl;
            }

            /* Logging */
            void log(const std::string &msg) const
            {
                // log_file << msg << std::endl;
            }
        };
    }

    namespace ReserveTHP
    {
        struct MetricsPolicy; // forward declared elsewhere
    }
}

template <>
struct BuddyPolicyFor<Ramulator::ReserveTHP::MetricsPolicy>
{
    using type = Ramulator::Buddy::MetricsPolicy;
};

template <>
struct BuddyPolicyFor<Ramulator::Baseline::MetricsPolicy>
{
    using type = Ramulator::Buddy::MetricsPolicy;
};