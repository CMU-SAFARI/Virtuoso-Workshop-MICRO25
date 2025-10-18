# Part 0 – Setup and Build Virtuoso + Sniper

This tutorial guides you through installing dependencies and compiling the **Virtuoso** simulator.

---

## 0.1 Prerequisites

1) Ask from the workshop organizers for access to a VM.
Send an email to: virtuosomicro25@gmail.com with your name and affiliation to get access to a pre-configured VM.
You can directly proceed to step 0.4 to build the simulator.

2) Before building Virtuoso, ensure your system meets the following requirements:
(if you want to build locally)

### Hardware
- **Architecture:** 64-bit x86 (x86-64)
- **Memory:** ≥ 4 GB (13 GB recommended)
- **Storage:** ≥ 10 GB free space

### Software
- **Operating System:** Linux (Ubuntu 20.04 or newer recommended)
- **Python:** 3.8 or later  
- **C/C++ Compiler:** GCC or Clang  
- **Libraries:**
  - `build-essential`
  - `python3-dev`
  - `libboost-all-dev`
  - `scons`
  - `git`
  - `wget`
  - `cmake`

You can install them on Ubuntu/Debian via:
the following command:

```bash
cd /path/to/Virtuoso-Workshop-MICRO25/virtuoso
sh install_dependencies.sh
```


---

## 0.4 Build the Simulator

If you haven't already, clone the Virtuoso repository:

```bash
git clone https://github.com/CMU-SAFARI/Virtuoso-Workshop-MICRO25.git
git checkout Part1.0-1.1
cd Virtuoso-Workshop-MICRO25/virtuoso
```

With all dependencies installed, build the simulator:

```bash
make distclean       # optional, cleans previous builds
make -j              # build using multiple cores
```

This process may take a few minutes depending on your system.

---



## 0.5 Verify the Build

Run the provided example to confirm the simulator works correctly:

```bash
sh run_example.sh
```

If it completes successfully without errors, Virtuoso has been installed and compiled correctly.

---
## 0.6 Explore the Codebase - Changing Configs
Familiarize yourself with the Virtuoso codebase, especially the configuration files located in `config/virtuoso_configs/`. You can modify these files to experiment with different simulation parameters.

**Try to add a new configuration file** by copying an existing one and changing a parameter, e.g., changing the cache size or page size settings.

--- 

## ✅ Next Step

You’re now ready for **Part 1**, where we will guide you through page size prediction. Proceed to [Part 1 – Latency-Based Huge Page Promotion](LatencyPromotion_Part1.1.md).

---

**Tip:** If you plan to modify the simulator frequently, consider adding this alias to your shell for convenience:

```bash
alias vmake='cd /path/to/Virtuoso-Workshop-MICRO25/virtuoso/simulator/sniper && make -j'
```

This lets you rebuild Virtuoso quickly from anywhere.
