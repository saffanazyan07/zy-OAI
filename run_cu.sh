#!/bin/bash


cd ~/zy-OAI/cmake_targets/ran_build/build/ || { echo "Directory is not found!"; exit 1; }

sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/cu_gnb.conf --sa --rfsim
