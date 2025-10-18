/* 0 -> no debug, 1 -> debug, 2 -> detailed debug */

#pragma once

/* DEBUG levels */
#define DEBUG_NONE     0
#define DEBUG_BASIC    1
#define DEBUG_DETAILED 2

// Sniper specific
#define DEBUG_CORE                  DEBUG_NONE  /* 0, 1 or 2 */    // NOTE: very noisy in DETAILED
#define DEBUG_MEM_MANAGER           DEBUG_NONE  /* 0, 1 or 2 */ // NOTE: very noisy in DETAILED
#define DEBUG_MMU                   DEBUG_NONE  /* 0, 1 or 2 */

#define DEBUG_MAGIC_SERVER          DEBUG_NONE/* 0, 1 or 2 */
#define DEBUG_TRACE_THREAD          DEBUG_NONE /* 0, 1 or 2 */

// > Page Tables
#define DEBUG_PAGE_TABLE_RADIX      DEBUG_NONE /* 0, 1 or 2 */

// ROB Performance Model specific
#define DEBUG_ROB_TIMER             DEBUG_NONE /* 0, 1 or 2 */
#define DEBUG_PERF_MODEL            DEBUG_NONE /* 0, 1 or 2 */
#define DEBUG_ROB_PERF_MODEL        DEBUG_NONE /* 0, 1 or 2 */

// VirtuOS specific
#define DEBUG_VIRTUOS               DEBUG_NONE  /* 0, 1 or 2 */
#define DEBUG_VALINOR_PF_HANDLER    DEBUG_NONE  /* 0, 1 or 2 */

// common (i.e., phys memory allocators)
#define DEBUG_RESERVATION_THP       DEBUG_NONE  /* 0, 1 or 2 */
#define DEBUG_EXCEPTION_HANDLER     DEBUG_NONE  /* 0, 1 or 2 */

#define DEBUG_BUDDY                 DEBUG_NONE  /* 0, 1 or 2 */

#define DEBUG_BASELINE_ALLOCATOR    DEBUG_NONE /* 0, 1 or 2 */

// DEBUG_SIFT_READER is defined & used in sift_reader.cc