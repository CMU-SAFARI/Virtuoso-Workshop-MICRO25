#!/bin/bash


# This script runs the Sniper simulator with a specified configuration file and workload.
# Usage: ./run_example.sh 

# Choose the configuration file for the Sniper simulator
CONFIG_FILE=./config/virtuoso_configs/virtuoso_reservethp_page_size_pred.cfg

# Path to your executable
TRACE=./traces/rnd.sift

# ------------------------------------------------------------
# Parameters for the Sniper simulator
# -c: Configuration file
# -d: Output directory
# --genstats: Generate statistics
# -s: Stop condition (e.g., stop by instruction count)
# --: Indicates the end of Sniper options and the start of the workload
# WORKLOAD can be any workload/executable supported by Sniper, such as 'ls', 'grep', etc.
# TRACE can be specified if needed, e.g., --traces=./traces/name.sift
# We are going to be using traces as they can be executed faster than real workloads.

./run-sniper -c $CONFIG_FILE -d ./part1.0_pspred_thp_off -g --perf_model/reserve_thp_allocator/target_fragmentation=0.0 --genstats -s stop-by-icount:5000000 --traces=$TRACE
./run-sniper -c $CONFIG_FILE -d ./part1.0_pspred_thp_on  -g --perf_model/reserve_thp_allocator/target_fragmentation=0.1 --genstats -s stop-by-icount:5000000 --traces=$TRACE

#Check if the command was successful by looking for sim.stats in the output directory

if [ -f ./part1.0_pspred_thp_off/sim.stats ] && [ -f ./part1.0_pspred_thp_on/sim.stats ]; then
    echo "Simulation completed successfully. Output is in ./part1.0_pspred_thp_off and ./part1.0_pspred_thp_on."
else
    echo "Simulation failed. Check the configuration and workload."
fi
