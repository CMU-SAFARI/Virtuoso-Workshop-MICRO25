#!/bin/bash

echo "Building Vanilla Trace Generator"
cd $INTEGRATION_ROOT/trace_generator_vanilla
./build.sh

echo "Building Mimicos Trace Generator"
cd $INTEGRATION_ROOT/trace_generator_mimicos
./build.sh

echo "Building Ramulator2"
cd $INTEGRATION_ROOT/ramulator2
mkdir -p build
cd build
cmake ..
make -j4
cp ./ramulator2 ../ramulator2
cd ..

echo "Building MQSim"
cd $INTEGRATION_ROOT/MQSim
make -j4