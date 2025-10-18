#!/bin/bash


# This script runs the Sniper simulator with a specified configuration file and workload.
# Usage: ./run_example.sh 

# Choose the configuration file for the Sniper simulator
CONFIG_FILE=./config/virtuoso_configs/virtuoso_reservethp.cfg

# Path to your executable
WORKLOAD=ls

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

# ./run-sniper -c $CONFIG_FILE -d ./example_output --genstats -s stop-by-icount:1000000 -- $WORKLOAD 
./run-sniper -c $CONFIG_FILE -d ./example_output --genstats -s stop-by-icount:1000000 --traces=./traces/randacc_8gb.sift

# Uncomment the following line if you want to test with real workloads
# ./run-sniper -c $CONFIG_FILE -d ./example_output --genstats -s stop-by-icount:1000000 -- $WORKLOAD 



#Check if the command was successful by looking for sim.stats in the output directory

if [ -f ./example_output/sim.stats ]; then
    echo "Simulation completed successfully. Output is in ./example_output."
else
    echo "Simulation failed. Check the configuration and workload."
fi
