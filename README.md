# Virtuoso Workshop – MICRO 2025

Welcome to the **Virtuoso Workshop** at MICRO 2025.  
The first part of the tutorial will guide you through setting up Virtuoso, understanding its Memory Management Unit (MMU), and exploring advanced memory management mechanisms such as **page size prediction** and **latency-based huge page promotion**.

---

##  Table of Contents
1. [Part 0 – Setup and Build Virtuoso + Sniper](#part-0--setup-and-build-virtuoso--sniper)
2. [Part 1.0 – Page Size Prediction](#part-10--page-size-prediction)
3. [Part 1.1 – Latency-Based Huge Page Promotion](#part-11--latency-based-huge-page-promotion)
4. [Appendix – Baseline MMU Overview](#appendix--baseline-mmu-overview)

---

## Part 0 – Setup and Build Virtuoso + Sniper

This part covers installing dependencies and building the **Virtuoso simulator**.

### 0.1 Prerequisites

1. **Workshop VM**  
   Request access to a preconfigured VM by emailing:  
   📧 `virtuosomicro25@gmail.com` (include your name and affiliation).  
   Once you have access, skip directly to **Step 0.4**.

2. **Local Installation (optional)**  
   Ensure your system meets the following requirements:

   **Hardware**
   - 64-bit x86 architecture  
   - ≥ 4 GB memory (13 GB recommended)  
   - ≥ 10 GB free storage  

   **Software**
   - Linux (Ubuntu 20.04+ recommended)  
   - Python ≥ 3.8  
   - GCC/Clang, `build-essential`, `libboost-all-dev`, `scons`, `cmake`, `git`, `wget`  

   Install dependencies:
   ```bash
   cd /path/to/Virtuoso-Workshop-MICRO25/virtuoso
   sh install_dependencies.sh
   ```

### 0.4 Build the Simulator

```bash
git clone https://github.com/CMU-SAFARI/Virtuoso-Workshop-MICRO25.git
git checkout Part1.0-1.1
cd Virtuoso-Workshop-MICRO25/virtuoso
make -j
```

### 0.5 Verify the Build

Run the provided example:
```bash
sh run_example.sh
```
If it completes without errors, Virtuoso is installed successfully.

### 0.6 Explore and Modify Configurations

Explore `config/virtuoso_configs/` and try modifying parameters such as cache or page size.  
You can quickly rebuild Virtuoso using this alias:
```bash
alias vmake='cd /path/to/Virtuoso-Workshop-MICRO25/virtuoso/simulator/sniper && make -j'
```

Next: [Part 1 – Latency-Based Huge Page Promotion](LatencyPromotion_Part1.1.md)

---

## Appendix – Baseline MMU Overview

Virtuoso models a **Memory Management Unit (MMU)** that performs virtual-to-physical address translation, including **TLB lookups** and **page table walks (PTWs)**.

### Key Components

- **`instantiatePageTableWalker()`**  
  Initializes page table walkers and walk caches for parallel PTWs.  

- **`instantiateTLBSubsystem()`**  
  Builds the hierarchical TLBs (L1, L2) that store recent address translations.  

- **`performAddressTranslation()`**  
  Main function for translating virtual to physical addresses.  
  It checks the TLBs, triggers a PTW on misses, and updates timing statistics.  

- **`registerMMUStats()`**  
  Registers metrics like translation latency and page fault counts.

### Translation Flow Summary

1. **TLB Lookup:** Iterate over TLB hierarchy for a hit.  
2. **PTW Trigger:** On miss, allocate a page table walker, measure latency.  
3. **Fault Handling:** Charge static latency for page faults (e.g., 1000 cycles).  
4. **TLB Fill:** Allocate new entries into the TLB that missed.  
5. **Performance Model Update:** Advance simulation time based on latency.

---

## Part 1.0 – Page Size Prediction

This section introduces the **Page Size Predictor**, integrated into Virtuoso’s TLB subsystem.

### Overview

The predictor uses a **history-based mechanism** inspired by the *TAGE* branch predictor to guess whether a virtual address maps to a 4KB or 2MB page.

#### Key Classes

- **`PageSizePredictor`** – Implements the prediction and training logic.  
- **`TLBHierarchy`** – Integrates the predictor into the TLB subsystem.  
- **`MemoryManagementUnit` (MMU)** – Uses predictions to optimize TLB lookups.

#### Prediction Logic

1. Maintain a 4-entry history (`12` for 4KB, `21` for 2MB).  
2. Use 16 tables of 32 bits each.  
3. Compute indices using the virtual address and history bits.  
4. Predict 2MB if any table bit is set; otherwise, 4KB.  
5. Train on real page sizes after PTWs to improve future predictions.

#### Integration Flow

- On an L1 TLB miss → `MMU` calls `predictPagesize(address)`.  
- Query the predicted-sized L2 TLB entry.  
- After the actual PTW result → `updatePageSizePredictor(address, actual_page_size)`.

### Run the Example

```bash
sh run_part1.0.sh
```

This runs two simulations:
```bash
# 1. Without prediction
./run-sniper -d ./part1_pspred_thp_off --perf_model/reserve_thp_allocator/target_fragmentation=0.0 ...

# 2. With prediction
./run-sniper -d ./part1_pspred_thp_on  --perf_model/reserve_thp_allocator/target_fragmentation=0.1 ...
```

Compare `sim.stats` in both directories to observe the prediction effect.

---

## Part 1.1 – Latency-Based Huge Page Promotion

This section describes **latency-driven promotion** of 4KB pages into 2MB huge pages based on measured access latencies.

### Workflow Overview

1. **Address Translation**  
   The MMU performs translations and measures TLB and PTW latencies.

2. **Latency Feedback**  
   After translation:
   ```cpp
   Sim()->getMimicOS()->getMemoryAllocator()->metadataUpdate(address, latency, core_id);
   ```
   The allocator records this in its `two_mb_map` for the corresponding 2MB region.

3. **Promotion Check on Page Fault**
   When allocating a new 4KB page:
   ```cpp
   bool ready_to_promote = (utilization > threshold) ||
                           (latency_4kb > promotion_threshold_latency4kb);
   ```
   Promotion occurs if either utilization or latency exceeds a threshold.

4. **Promotion Threshold**
   Set in `reserve_thp.cfg`:
   ```ini
   [perf_model/reserve_thp_allocator]
   promotion_threshold_latency_ns = 10000
   ```

### Testing Latency-Aware Promotion

```bash
./run-sniper -d ./part1.1_reservethp_util   --perf_model/reserve_thp_allocator/enable_latency_aware_promotion=false ...
./run-sniper -d ./part1.1_reservethp_latency --perf_model/reserve_thp_allocator/enable_latency_aware_promotion=true ...
```

Compare results to see how latency-aware promotion reduces memory access time and improves simulation performance.

---

##  Summary

| Component | Purpose | Key File |
|------------|----------|----------|
| Virtuoso Build | Sets up simulator | `install_dependencies.sh`, `Makefile` |
| MMU | Performs translation | `mmu.cc` |
| Page Size Predictor | Predicts 4KB vs 2MB | `page_size_predictor.cc` |
| Latency-Based Promotion | Promotes based on latency | `reserve_thp.cc` |

