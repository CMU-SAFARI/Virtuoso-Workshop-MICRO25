#pragma once

#include <fstream>
#include <bitset>
#include <map>
#include <tuple>

#include "fixed_types.h"

namespace Ramulator
{
    namespace ReserveTHP
    {
        struct MetricsPolicy
        {
            mutable std::ofstream log_file;

            ~MetricsPolicy()
            {
                if (log_file.is_open())
                    log_file.close();
            }

            template <typename Allocator>
            void on_init(const String &name, int memory_size, int kernel_size, int threshold_for_promotion, Allocator *phys_mem_alloc)
            {
                // auto &stats = phys_mem_alloc->getStats();
                // std::string log_file_name = std::string(Sim()->getConfig()->getOutputDirectory().c_str()) + "/reservation_thp.log";
                // log_file.open(log_file_name);
                // if (!log_file.is_open())
                // {
                //     throw std::runtime_error("[RESERVE_THP_POLICY] Failed to open log file");
                // }

                // std::cout << "[MimicOS] Reservation-based THP Allocator" << std::endl;
                // std::cout << "[MimicOS] ReserveTHP: threshold_for_promotion = " << threshold_for_promotion << std::endl;

                // log_file << "[MimicOS] Creating Reservation-based THP Allocator" << std::endl;
                // registerStatsMetric(name, 0, "four_kb_allocated", &stats.four_kb_allocated);
                // registerStatsMetric(name, 0, "two_mb_reserved", &stats.two_mb_reserved);
                // registerStatsMetric(name, 0, "two_mb_promoted", &stats.two_mb_promoted);
                // registerStatsMetric(name, 0, "two_mb_demoted", &stats.two_mb_demoted);
                // registerStatsMetric(name, 0, "total_allocations", &stats.total_allocations);
                // registerStatsMetric(name, 0, "page_table_pages_used", &stats.kernel_pages_used);
            }

            /* Logging */
            void log(const std::string &msg) const
            {
                // log_file << msg << std::endl;
            }
        };
    }
}