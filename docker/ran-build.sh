#!/bin/bash

set -xe

# any options to this script are transparently passed to build_oai
BUILD_OPTION=${@}

mkdir -p cmake_targets/log
cd cmake_targets
./build_oai --ninja \
  --eNB --gNB --RU --UE --nrUE \
  --build-lib "telnetsrv enbscope uescope nrscope" \
  -w USRP -t Ethernet \
  --noavx512 ${BUILD_OPTION}
  #--build-e2 --cmake-opt -DXAPP_MULTILANGUAGE=OFF \
  #--cmake-opt -DCMAKE_C_FLAGS="-Werror" --cmake-opt -DCMAKE_CXX_FLAGS="-Werror" ${BUILD_OPTION}
