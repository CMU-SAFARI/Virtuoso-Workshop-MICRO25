#!/bin/bash


# This script runs the Sniper simulator with a specified configuration file and workload.
# Usage: ./run_example.sh 


# Path to your executable
WORKLOAD=</path/to/your/workload>  # Replace with your actual workload, e.g., 'ls', 'grep', etc.
 

# "" Lab 4: Recording a Microbenchmark Trace
# ------------------------------------------------------------
# record-trace is a tool used to record traces of microbenchmarks.
# It captures the execution of a workload and generates a trace file that can be used for analysis
# or simulation purposes.
# -d: How many instructions to record
# -o: Output directory for the trace file
# --roi: Indicates that the trace should be recorded only during the region of interest (ROI)
# --: Indicates the end of options for record-trace, followed by the workload to be executed

../record-trace -d 50000000 -o ./microbenchmark --roi -- $WORKLOAD 

# This command will generate a trace file inside the `./microbenchmark` directory.



