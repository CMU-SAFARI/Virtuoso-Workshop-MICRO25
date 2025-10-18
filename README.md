# Part 2: Exploring Virtuoso's Agility and Flexibility

In this demonstration, we will demonstrate (i) how Virtuoso enables rapid prototyping of complex OS features such as swap space and (ii) allows flexibility in interfacing with various simulators such as Ramulator2.0 (a main memory device simulator) and MQSim (a storage device simulator).

## Workflow

This demonstration uses traces from an Intel Pin-based trace generator. These traces consist of non-memory instructions modelled using bubble counts and memory instructions such as load/store operations. 

**Page Fault Overheads**
We model the overheads introduced due to page faults and swapping through a lightweight implementation of MimicOS. This tool provides a trace with all main memory accesses and a trace with storage accesses.

**Unified Simulation**
We interface the Virtuoso simulation with a main memory simulator, Ramulator2.0 and a storage system simulator, MQSim. 


## Part 1: Modelling Page Fault and Swap Overheads

The original trace lacks page fault and swap space accesses. We model OS routines such as allocation and swap space in order to incorporate main memory and storage accesses. 

We do this by introducing magic instructions that signal a trace generator to start and stop recording. In the magic section, we model the allocation and swapping routines. 

```cpp
// Start trace collection
SimMagic0(38);

// Stop trace collection
SimMagic0(0);
```

To show Virtuoso's abilities to enable rapid prototyping of OS features such as swap space support, we implement a 3-layer Neural Network to predict a page replacement candidate. We modify CLOCK-Pro to select top K candidates for replacement. The K candidate features are passed to a 3-layer Neural Network to predict the candidate for replacement. 

```cpp
// Find victim candidates to swap out
std::vector<PT::iterator> victim_candidates = get_victim_candidates(pagetable, hand);

// ... create the input candidate features

// Predict the victim index
int victim_idx = nn_evictor->predict(inputs);
```

## Part 2: Modelling Main Memory and Storage Performance

To model the main memory and storage performance impacts of virtual memory support, we dump the main memory and swap space accesses into a Ramulator2 and MQSim compatible formats.

## Instructions

### Setup Environment Variables and Build

```bash
export INTEGRATION_ROOT=`pwd`
export PIN_ROOT=$INTEGRATION_ROOT/pin-external-3.31-98869-gfa6f126a8-gcc-linux
./build.sh
```

### Generate Ramulator2.0 and MQSim Compatible Traces

```bash
cd $INTEGRATION_ROOT/trace_generator_mimicos/
$PIN_ROOT/pin -t src/obj-intel64/gettrace.so -c src/Cache.cfg -- ./virtuos ../vtw25.trace ../final_vtw25_r2.trace ../final_vtw25_mqsim.trace
```

### Perform Ramulator2.0 and MQSim Simulations

```bash
cd $INTEGRATION_ROOT/ramulator2/
./ramulator2 -f ../config_replay.yaml

cd $INTEGRATION_ROOT/MQSim
./MQSim -i ../pf_ssdconfig.xml -w ../pf_workload.xml
```