

## Overview

In modern systems, memory is managed in units called pages. While 4KB pages are common, using larger pages (like 2MB Transparent Huge Pages) can improve performance by reducing the number of TLB (Translation Lookaside Buffer) misses and the overhead of page table walks.

Virtuoso implements a sophisticated memory allocator, `ReservationTHPAllocator`, which can dynamically "promote" a 2MB region of memory that is currently being accessed as individual 4KB pages into a single 2MB huge page. This decision is based on two criteria:

1.  **Utilization:** If a certain percentage of the 4KB pages within a 2MB virtual address range are being used, the region is promoted.
2.  **Latency:** If the cumulative time spent on address translation for 4KB pages within a 2MB region exceeds a certain threshold, the region is promoted.

This document focuses on the **latency-based promotion path**.

## The Promotion Workflow

The process involves the Memory Management Unit (MMU) and the `ReservationTHPAllocator`. Here is a step-by-step breakdown:

### 1. Address Translation and Latency Calculation

-   When the CPU needs to access a memory address, it sends a request to the **MMU** (`mmu.cc`).
-   The MMU's `performAddressTranslation` function is responsible for translating the virtual address to a physical address.
-   This process involves checking the TLB hierarchy. If it's a TLB miss, a **page table walk** is initiated.
-   The MMU calculates the total latency for this translation (`charged_tlb_latency + total_walk_latency`). This represents the overhead of handling this memory access at a 4KB granularity.

### 2. Updating Latency Metadata

-   After each translation, the MMU updates the memory allocator with the calculated latency. It calls:
    ```cpp
    // In mmu.cc, at the end of performAddressTranslation
    Sim()->getMimicOS()->getMemoryAllocator()->metadataUpdate(address, charged_tlb_latency + total_walk_latency, getCore()->getId());
    ```
-   The `ReservationTHPAllocator` receives this latency in its `metadataUpdate` function (`reserve_thp.cc`).
-   It identifies the 2MB virtual region the address belongs to and adds the new latency to a running total for that region. This is stored in the `two_mb_map` data structure.

    ```cpp
    // In reserve_thp.cc
    void ReservationTHPAllocator::metadataUpdate(IntPtr address, SubsecondTime latency_4kb, UInt64 core_id)
    {
        UInt64 region_2MB = address >> 21;
        std::get<3>(two_mb_map[region_2MB]) += latency_4kb;
    }
    ```

### 3. Checking for Promotion on a Page Fault

-   When a page fault occurs for a new 4KB page within a 2MB region, the `ReservationTHPAllocator::allocate` function is called.
-   This function calls `checkFor2MBAllocation` to handle the allocation and check if a promotion is warranted.
-   Inside `checkFor2MBAllocation`, the allocator retrieves the accumulated latency for the 2MB region.

### 4. The Promotion Decision

-   The core of the latency-based promotion logic is the following check:

    ```cpp
    // In reserve_thp.cc's checkFor2MBAllocation
    float utilization = static_cast<float>(bitset.count()) / 512;
    SubsecondTime latency_4kb = std::get<3>(region);
    bool ready_to_promote = (utilization > threshold_for_promotion) || (latency_4kb > promotion_threshold_latency4kb);
    ```

-   The region is "ready to promote" if either the utilization threshold is met OR the accumulated `latency_4kb` for that region exceeds the `promotion_threshold_latency4kb`.

-   This `promotion_threshold_latency4kb` is a configurable value set in `reserve_thp.cfg`:

    ```ini
    [perf_model/reserve_thp_allocator]
    promotion_threshold_latency_ns = 10000 # Latency for promotion threshold (in ns)
    ```

-   If `ready_to_promote` is true, the allocator "promotes" the region. This means that from this point on, the entire 2MB region will be mapped as a single huge page, which should lead to faster subsequent address translations for any address within that region.


