# Part 1.2.1: MimicOS Runtime: Software-based Page Faults


This document explains the responsibilities and control flow of the provided `MimicOS` source file. It focuses on: configuration, allocator setup, simulator hand-off, the message protocol, and the main allocation loop that services page faults.

---

## What this component is

**MimicOS** is a minimal userspace OS that runs alongside a SIFT/Sniper-like simulator. 
For simplicity and demonstration purposes, we configure it to only handle page faults and allocate physical memory frames on demand for a single 
process.

- Parses a config file (INI) to build a **physical memory allocator**.
- **Boots** a traced user application inside the simulator.
- **Receives allocation requests** from the simulator (typically on page faults).
- **Allocates frames** (data frame + page-table frames) and returns the results to the app via a **message protocol** and **magic** simulator calls.

---

## Key includes & globals

- `INIReader *reader;` – global configuration reader (from `globals.h`).
- `MetricsRegistry *m_stats;` – global metrics registry (from `globals.h`).
- `physical_memory_allocator` – created via `AllocatorFactory` from config.

---

## Construction: configuration & allocator

```cpp
MimicOS::MimicOS(std::string configurationFile, std::string outputFile, std::string appFile)
  : path_to_outputFile(outputFile),
    path_to_app(appFile),
    path_to_configFile(configurationFile)
{
    reader = new INIReader(configurationFile);
    m_stats = new MetricsRegistry();

    String allocatorName = reader->Get("allocator", "memory_allocator", "").c_str();
    int maxOrder = reader->GetInteger("allocator", "max_order", 0);
    int kernelSize = reader->GetInteger("pmem_alloc", "kernel_size", 0);
    String fragType = reader->Get("pmem_alloc", "frag_type", "none").c_str();
    int memory_size = reader->GetInteger("pmem_alloc", "memory_size", 0);
    int threshold_for_promotion =
        reader->GetInteger("pmem_alloc", "threshold_for_promotion", -1);

    physical_memory_allocator =
        AllocatorFactory::createAllocator(allocatorName, memory_size,
                                          maxOrder, kernelSize, fragType,
                                          threshold_for_promotion);
}
```

Where do we get these values from? The **INI config file** passed as `configurationFile` argument.

Example: `configs/reservethp_32GB.ini`

```ini
[allocator]
memory_allocator=reserve_thp
max_order = 12

[pmem_alloc]
memory_size = 131072
target_fragmentation = 1.0
target_memory = 0.0
fragmentation_file = ""
kernel_size = 32768
max_order = 12
frag_type = largepage
threshold_for_promotion= 0.0
```

### Config keys (INI)

| Section       | Key                       | Meaning                                                                                   |
|----------------|----------------------------|--------------------------------------------------------------------------------------------|
| `[allocator]`  | `memory_allocator`         | Allocator type (e.g., `Baseline`, `ReserveTHP`, etc.).                                    |
| `[allocator]`  | `max_order`                | Buddy max order (only for buddy-based allocators).                                        |
| `[pmem_alloc]` | `kernel_size`              | Reserve first N MB for “kernel” space.                                                    |
| `[pmem_alloc]` | `frag_type`                | Fragmentation objective (`contiguity`, `large_pages`, or `none`).                         |
| `[pmem_alloc]` | `memory_size`              | Total simulated physical memory (MB or allocator-specific units).                         |
| `[pmem_alloc]` | `threshold_for_promotion`  | For ReserveTHP: fraction of 4 KiB usage to promote to 2 MiB.                              |
| `[pmem_alloc]` | `target_fragmentation`     | (Used in `boot()`) Target fragmentation level; 1.0 ⇒ no fragmentation.                    |

> 💡 **ReserveTHP** knobs (`max_order`, `threshold_for_promotion`) only matter for allocators that implement promotion/buddy coalescing semantics.

---

## Boot sequence

```cpp
void MimicOS::boot() {
    double target_fragmentation = reader->GetReal("pmem_alloc", "target_fragmentation", 1.0);
    physical_memory_allocator->fragment_memory(target_fragmentation);

    start_application();   // Tell simulator to start the traced app
    poll_for_signal();     // Enter main allocation loop (does not return)
}
```

1. **Optional fragmentation** to a target factor (≤ 1.0 applies fragmentation).
2. **Start application** via simulator (ROI + magic start).
3. **Enter server loop** to handle memory exceptions/requests forever.

---

## Starting the traced app

```cpp
void MimicOS::start_application() {
    const char *path = path_to_app.c_str();
    SimRoiStart();                          // Delimit region of interest
    SimStartProcess((long unsigned int)path); // “Magic” to start process in sim
}
```

- `SimRoiStart()` marks a region of interest for stats.
- `SimStartProcess()` is a “magic” call: the simulator intercepts and starts the process at `path_to_app`.

---

## Message protocol

**Incoming (from simulator → MimicOS)**

```
argv[0] = exception_type_code        // e.g., page fault type
argv[1] = virtual_address            // faulting VA
argv[2] = num_requested_frames       // 1 data frame + N page-table frames
argv[3..] = (unused)
argc    >= 3
```

**Outgoing (MimicOS → simulator/app)**

```
argv[0] = exception_type_code        // echo back
argv[1] = vpn                        // VA >> 12
argv[2] = pa                         // physical address of allocated data frame
argv[3] = page_size                  // e.g., 12 for 4 KiB
argv[4..(4+N-1)] = frames[0..N-1]    // PA of all frames (data first, then PT frames)
argc = 4 + num_requested_frames
```

> The **first frame** in `frames` is the **data frame**; the rest are **page table frames**.

---

## The main loop: `poll_for_signal()`

```cpp
void MimicOS::poll_for_signal() {
    Message* msg = new Message;
    msg->argv = new uint64_t[10];

    SimContextSwitch();  // Yield to app; we’ll get woken on first request

    while (true) {
        // 1) Receive request
        SimReceiveMessage(&msg->argc, msg->argv);
        assert(msg->argc >= 2);

        int exception_type_code = msg->argv[0];
        IntPtr va = msg->argv[1];
        IntPtr vpn = (va >> BASE_PAGE_SHIFT);
        int num_requested_frames = msg->argv[2];

        // 2) Allocate frames
        const UInt64 bytes = (1 << 12); // 4 KiB request unit
        int core_id = 0;                // Simplification: single core

        std::vector<UInt64> frames;
        frames.reserve(num_requested_frames);

        // 2a) Data frame
        auto [pa, page_size] = physical_memory_allocator->allocate(bytes, va, core_id);
        if (pa == static_cast<UInt64>(-1)) {
            std::cerr << "[FATAL] No more memory";
            exit(1);
        }
        frames.push_back(pa);

        // 2b) Page-table frames
        for (int i = 0; i < num_requested_frames - 1; i++) {
            auto frame = physical_memory_allocator->handle_page_table_allocations(bytes);
            frames.push_back(frame);
        }

        // 3) Build response
        msg->argc = 4 + num_requested_frames;
        msg->argv[0] = exception_type_code;
        msg->argv[1] = vpn;
        msg->argv[2] = pa;
        msg->argv[3] = page_size;
        for (int i = 0; i < num_requested_frames; i++)
            msg->argv[4 + i] = frames[i];

        // 4) Send back + context switch to resume app
        SimMimicosResult(msg->argc, msg->argv);
        SimContextSwitch();
    }
}
```

### Step-by-step 

1. **Wait** for the app to run and fault (`SimContextSwitch()`).
2. **Receive** a request describing the exception and number of frames.
3. **Compute VPN** by shifting VA by 12.
4. **Allocate** the **data frame** via allocator.
5. **Allocate** any **page-table frames**.
6. **Respond** with `(ex, vpn, pa, page_size, frames...)`.
7. **Yield** back to app with `SimContextSwitch()`.


```
App   → Sim        : page fault at VA
Sim   → MimicOS    : [ex, VA, #frames]
MimicOS → Allocator: allocate data (4KiB)
MimicOS → Allocator: allocate N-1 PT frames
MimicOS → Sim      : [ex, VPN, PA_data, page_size, PA_all_frames...]
Sim   → App        : resumes; maps frames
```

---

## Error handling & debug

- **Out-of-memory**: fatal exit with message.
- **Protocol checks**: `assert(msg->argc >= 2)`.
- **Debug logging** (controlled via `DEBUG_MimicOS`) prints request/response and frame allocation info.

---

## Design choices & implications

- **Single-core**: currently fixed `core_id = 0`. Extend protocol for multi-core simulation.
- **Allocator authority**: Allocator may return larger frames (e.g., 2 MiB).
- **Fragmentation**: Applied only pre-boot; runtime fragmentation depends on allocation policy.
- **Metrics**: `m_stats` initialized but unused; can track per-allocation statistics.

---

## Gotchas & tips

- **Message buffer**: `argv` has length 10 — ensure `num_requested_frames` doesn’t overflow `argc`.
- **Log2 page size**: `page_size` encodes log2(bytes) (12 ⇒ 4 KiB).
- **Alignment**: Allocator must return aligned PAs.
- **Fragmentation mode**: Check allocator’s interpretation of `frag_type`.

---

## Example INI (minimal)

```ini
[allocator]
memory_allocator = ReserveTHP
max_order = 9

[pmem_alloc]
memory_size = 16384             ; 16 GiB
kernel_size = 128               ; 128 MB reserved for kernel
frag_type = large_pages
threshold_for_promotion = 70    ; promote when 70% used
target_fragmentation = 0.85     ; apply pre-boot fragmentation
```

---

## Simulator API reference

| Function              | Description |
|-----------------------|--------------|
| `SimRoiStart()`       | Marks ROI for simulation stats. |
| `SimStartProcess()`   | “Magic” instruction to start app process. |
| `SimContextSwitch()`  | Yields control between app and MimicOS. |
| `SimReceiveMessage()` | Receives message from app (e.g., page fault). |
| `SimMimicosResult()`  | Sends result (allocated frames) back to app. |

---


## Handling Page Faults in MimicOS

MimicOS handles page faults by simulating the behavior of a real operating system. When a page fault occurs, the following steps are taken:

(1) ReserveTHP tries to allocate a 2 MiB page. If successful, it returns the physical address of the 2 MiB page.

```cpp
auto [pa, page_size] = physical_memory_allocator->allocate(bytes, va, core_id);
```

(2) If the allocation of a 2 MiB page fails, MimicOS falls back to allocating a 4 KiB page.

```cpp
auto [pa, page_size] = physical_memory_allocator->allocate(4 * 1024, va, core_id);

(1) and (2) are similar to what we showed in Part 1.1 with the ReserveTHP allocator.

(3) MimicOS also allocates/updates any necessary page table pages to map the newly allocated data page.

```cpp
for (int i = 0; i < num_requested_frames - 1; i++) {
    auto frame = physical_memory_allocator->handle_page_table_allocations(bytes);
    frames.push_back(frame);
}

(4) MimicOS zeroes out the contents of the newly allocated data page to ensure it is clean.

```cpp
memset((void*)pa, 0, bytes);
```

(5) Finally, MimicOS sends the physical addresses of the allocated pages back to the simulator, which then maps them into the application's address space.

```cpp
msg->argv[0] = exception_type_code;
msg->argv[1] = vpn;
msg->argv[2] = pa;
msg->argv[3] = page_size;
for (int i = 0; i < num_requested_frames; i++)
    msg->argv[4 + i] = frames[i];
``` 


# Performance Overheads of Minor Page Faults

Minor page faults handled by MimicOS enable simulating the overheads of page fault handling in a real OS. The performance overheads include:

- **Page table updates**: Each page fault may require updating multiple page table entries.
- **Memory initialization**: Newly allocated pages must be zeroed out, adding overhead.
- **Context switching**: Handling page faults may involve switching between processes or threads.

## Let's run two experiments to see the performance overheads of MimicOS handling page faults:

```bash
# Experiment 1: Run with MimicOS handling page faults
sh run_mimicos_part_1.2.1.sh
# Experiment 2: Run without MimicOS handling page faults (baseline)
sh run_baseline_part_1.2.1.sh
```



# Part 1.2.2: Offloading Page Fault Handling to Hardware

This section describes how to offload page fault handling to a hardware fault handler (HWFaultHandler) in the MimicOS runtime. The HWFaultHandler is responsible for managing page faults and allocating memory frames directly, reducing the overhead of software-based page fault handling.

## Workflow Overview
1. **MMU with HWFaultHandler**  
   The MMU is configured to use the HWFaultHandler for handling page faults.
2. **HWFaultHandler Initialization**
    The HWFaultHandler is instantiated and allocated memory for its internal structures.
3. **Page Fault Handling**  
   When a page fault occurs, the HWFaultHandler translates the virtual address and allocates
    the necessary memory frames.



## Let's run two experiments to see the performance of MimicOS with hardware fault handling:

```bash
# Experiment 1: Run with MimicOS using hardware fault handler
sh run_mimicos_hwfaults_part_1.2.2.sh
# Experiment 2: Run without hardware fault handler (baseline)
sh run_mimicos_part_1.2.1.sh
```
