#!/bin/bash
cd cmake_targets/ran_build/build/ || { echo "Directory is not found!"; exit 1; }
sudo ./z-cu -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/cu_gnb.conf --gNBs.[0].enable_sdap 1 --gNBs.[0].drbs 2 --sa --rfsim
