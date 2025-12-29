#!/bin/bash
cd cmake_targets/ran_build/build/ || { echo "Directory is not found!"; exit 1; }
sudo ./d-cu -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-cu.sa.f1.conf --telnetsrv --telnetsrv.shrmod ci
