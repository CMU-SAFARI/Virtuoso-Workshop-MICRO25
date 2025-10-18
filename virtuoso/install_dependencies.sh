#!/bin/bash

# Exit immediately if a command exits with a non-zero status

set -e
path=$(pwd)


echo "Installing dependencies for Virtuoso..."

sudo apt-get update
sudo apt-get install -y binutils build-essential curl git libboost-dev libbz2-dev libc6:i386 libncurses5:i386 libsqlite3-dev libstdc++6:i386 wget zlib1g-dev
sudo apt-get install -y python3