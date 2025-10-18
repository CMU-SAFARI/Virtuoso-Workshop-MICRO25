#!/bin/bash


# This script runs the Sniper simulator with a specified configuration file and workload.
# Usage: ./run_example.sh 

# Choose the configuration file for the Sniper simulator
CONFIG_FILE=./config/virtuoso_configs/virtuoso_reservethp_latency_aware.cfg

# Path to your executable
TRACE=./traces/bc.sift

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

./run-sniper -c $CONFIG_FILE -d ./part1.1_reservethp_util -g --perf_model/reserve_thp_allocator/enable_latency_aware_promotion=false  --genstats -s stop-by-icount:3000000 --traces=$TRACE
./run-sniper -c $CONFIG_FILE -d ./part1.1_reservethp_latency -g --perf_model/reserve_thp_allocator/enable_latency_aware_promotion=true --genstats -s stop-by-icount:3000000 --traces=$TRACE

#Check if the command was successful by looking for sim.stats in the output directory

if [ -f ./part1.1_reservethp_util/sim.stats ] && [ -f ./part1.1_reservethp_latency/sim.stats ]; then
    echo "Simulation completed successfully. Output is in ./part1.1_reservethp_util and ./part1.1_reservethp_latency."
else
    echo "Simulation failed. Check the configuration and workload."
fi
