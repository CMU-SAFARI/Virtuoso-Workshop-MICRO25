
## Overview

The page size predictor is a component of the Memory Management Unit (MMU) and is managed within the `TLBHierarchy`. It is used during the address translation process to guess the size of the page a virtual address belongs to.

The core components of the page size prediction mechanism are:

1.  **`PageSizePredictor` class**: This class implements the prediction logic.
2.  **`TLBHierarchy` class**: This class integrates the predictor into the TLB subsystem.
3.  **`MemoryManagementUnit` (MMU) class**: The MMU orchestrates the address translation and uses the `TLBHierarchy` to perform TLB lookups, which can involve page size prediction.

## The Predictor: `PageSizePredictor`

The `PageSizePredictor` uses a history-based approach to make predictions. It is based on the "Two-Level Adaptive History Component" from the TAGE branch predictor.

### Data Structures

-   **`history`**: A `std::deque<int>` that stores the page sizes of the last 4 translations. It stores `12` for a 4KB page and `21` for a 2MB page. This history is used to form an index.

-   **`tables`**: A `std::vector<std::vector<bool>>` representing 16 tables. Each table has 32 entries (bits). These tables store the prediction information.

### Prediction Logic (`predictPageSize`)

1.  The predictor takes a virtual address as input.
2.  It calculates an index into the history `tables` by using a combination of the virtual address and the recent history of page sizes.
3.  Specifically, it computes an index for each of the 16 tables. The index for table `i` is calculated as:
    `(virtual_address >> 12) ^ (history_index & ((1 << i) - 1))`
    The `history_index` is a 4-bit value derived from the `history` deque, where a '1' represents a 2MB page and a '0' represents a 4KB page.
4.  It looks up the bit at the calculated index in each of the 16 tables.
5.  The prediction is based on the values from the tables. If the bit is set in any of the tables, it predicts a 2MB page (`21`). Otherwise, it predicts a 4KB page (`12`).

### Update Logic (`update`)

After a page walk is completed and the actual page size is known, the `update` method is called to train the predictor.

1.  The `update` method receives the virtual address and the actual page size.
2.  It makes a prediction for the given virtual address, just like in `predictPageSize`.
3.  It compares the prediction with the actual page size.
4.  If the prediction was incorrect, it updates one of the tables. It flips the bit at the corresponding index in the first table that gave a wrong prediction. For example, if the predictor guessed 4KB (all table bits were 0) but the actual size was 2MB, it will set the bit in the first table. If it guessed 2MB but it was 4KB, it will clear the bit in the first table that had a '1'.
5.  Finally, it updates the `history` deque by pushing the new actual page size and removing the oldest entry.

## Integration in the Simulator

### `TLBHierarchy`

The `TLBHierarchy` is responsible for creating and managing the `PageSizePredictor`.

-   During its initialization, `TLBHierarchy` creates an instance of `PageSizePredictor` using the `PagesizePredictorFactory`.
-   It exposes two methods:
    -   `predictPagesize(IntPtr virtual_address)`: This method calls the predictor's `predictPageSize` method.
    -   `updatePageSizePredictor(IntPtr virtual_address, int page_size)`: This method calls the predictor's `update` method.

### `MemoryManagementUnit` (MMU)

The `MemoryManagementUnit` uses the `TLBHierarchy` to perform address translation. The page size predictor is used to decide whether to speculatively access the L2 TLB for a 2MB page entry.

The flow is as follows:

1.  When `performAddressTranslation` is called, the MMU first checks the L1 TLBs (for instructions and data).
2.  If there is a miss in the L1 TLB, the MMU needs to check the L2 TLB.
3.  The L2 TLB in this system can store entries for both 4KB and 2MB pages.
4.  The MMU calls `tlb_subsystem->predictPagesize(address)` to get a prediction for the page size.
5.  Based on the prediction, it will query the L2 TLB for an entry of the predicted size.
6.  After a page table walk, the actual page size is determined.
7.  The MMU then calls `tlb_subsystem->updatePageSizePredictor(address, actual_page_size)` to update the predictor with the correct information.


## Running the Page Size Prediction Example

```bash
sh run_part1.0.sh
```

This script runs two simulations:

1.  With page size prediction disabled (target fragmentation set to 0.0).
2.  With page size prediction enabled (target fragmentation set to 0.1 to allow large pages).

```bash 

./run-sniper -c $CONFIG_FILE -d ./part1_pspred_thp_off -g --perf_model/reserve_thp_allocator/target_fragmentation=0.0 --genstats -s stop-by-icount:5000000 --traces=$TRACE

./run-sniper -c $CONFIG_FILE -d ./part1_pspred_thp_on  -g --perf_model/reserve_thp_allocator/target_fragmentation=0.1 --genstats -s stop-by-icount:5000000 --traces=$TRACE

```

The output directories will be `part1_pspred_thp_off` and `part1_pspred_thp_on`, respectively. You can analyze the results in these directories to see the impact of page size prediction on performance.

Take a look at the `sim.stats` files in each output directory to compare metrics such as the charged TLB latency and the number of large pages allocated.


