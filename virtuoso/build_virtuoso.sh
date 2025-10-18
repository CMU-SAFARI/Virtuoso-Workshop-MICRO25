#!/bin/bash


make distclean 
make -j

if [ $? -ne 0 ]; then
    echo "Build failed. Please check the output for errors."
    exit 1
fi
echo "Build completed successfully."