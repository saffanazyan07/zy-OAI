/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file dfts_load.c
 * \brief: load library implementing coding/decoding algorithms
 * \author Francois TABURET
 * \date 2020
 * \version 0.1
 * \company NOKIA BellLabs France
 * \email: francois.taburet@nokia-bell-labs.com
 * \note
 * \warning
 */
#define _GNU_SOURCE 
#include <sys/types.h>
#include <stdlib.h>
#include <malloc.h>
#include "assertions.h"
#include "common/utils/LOG/log.h"
#define OAIDFTS_LOADER
#include "tools_defs.h"
#include "common/config/config_userapi.h" 
#include "common/utils/load_module_shlib.h" 

uint32_t DFT_SCALING_64[5][2]   = {{3,0},{2,1},{1,2},{1,2},{1,2}};
uint32_t DFT_SCALING_128[5][3]  = {{4,0,0},{3,1,0},{2,2,0},{1,3,0},{0,4,0}};
uint32_t DFT_SCALING_256[5][3]  = {{4,0,0},{3,1,0},{2,2,0},{1,3,0},{0,4,0}};
int32_t DFT_SCALING_512_THRES[7] = {53,57,59,63,65,69,100};
uint32_t DFT_SCALING_512[7][4]  = {{5,0,0,0},{4,1,0,0},{4,0,1,0},{3,1,1,0},{3,0,1,1},{2,1,1,1},{2,0,1,2}};
uint32_t DFT_SCALING_768[][4]   = {{1,2,2,0},{1,2,2,0},{1,2,2,0},{1,2,2,0},{1,2,2,0}};
int32_t DFT_SCALING_1024_THRES[5] = {49,55,63,69,100};
uint32_t DFT_SCALING_1024[5][4] = {{5,0,0,0},{4,1,0,0},{3,1,1,0},{2,1,1,1},{1,1,1,2}};
uint32_t DFT_SCALING_1536[5][5] = {{1,1,2,2,0},{1,1,2,2,0},{1,1,2,2,0},{1,1,2,2,0},{1,1,2,2,0}};
int32_t DFT_SCALING_2048_THRES[10] = {47,49,53,55,59,61,65,67,69,100};
uint32_t DFT_SCALING_2048[10][5] = {{1,1,1,1,2},{1,1,1,1,2},{1,1,1,1,2},{1,1,1,1,2},{1,1,1,1,2},{1,1,1,1,2},{1,1,1,1,2},{1,1,1,1,2},{1,1,1,1,2},{1,1,1,1,2}} ; //}{{6,0,0,0,0},{5,1,0,0,0},{5,0,1,0,0},{4,1,1,0,0},{4,0,1,1,0},{3,1,1,1,0},{3,0,1,1,1},{2,1,1,1,1},{2,0,2,0,2},{1,0,1,0,4}};
uint32_t DFT_SCALING_3072[5][5] = {{1,4,1,0,0},{1,0,3,2,0},{1,0,3,2,0},{1,0,3,2,0},{1,0,3,2,0}};
int32_t DFT_SCALING_4096_THRES[8] = {43,49,57,61,63,65,69,100};
uint32_t DFT_SCALING_4096[8][5] = {{6,0,0,0,0},{5,1,0,0,0},{4,1,1,0,0},{3,1,1,1,0},{2,2,0,1,1},{2,1,1,1,1},{1,1,2,1,1},{0,0,3,0,3}};
uint32_t DFT_SCALING_6144[5][6] = {{1,1,0,3,2,0},{1,1,0,3,2,0},{1,1,0,3,2,0},{1,1,0,3,2,0},{1,1,0,3,2,0}};
uint32_t DFT_SCALING_8192[5][6] = {{1,0,0,3,3,0},{1,0,0,3,3,0},{1,0,0,3,3,0},{1,0,0,3,3,0},{1,0,0,3,3,0}};
uint32_t DFT_SCALING_9216[5][6] = {{1,1,0,3,2,1},{1,1,0,3,2,0},{1,1,0,3,2,0},{1,1,0,3,2,0},{1,1,0,3,2,0}};
uint32_t DFT_SCALING_12288[5][6] = {{1,0,0,3,3,0},{1,0,0,3,3,0},{1,0,0,3,3,0},{1,0,0,3,3,0},{1,0,0,3,3,0}};
uint32_t DFT_SCALING_16384[5][6] = {{0,0,1,3,3,0},{0,0,1,3,3,0},{0,0,1,3,3,0},{0,0,1,3,3,0},{0,0,1,3,3,0}};
uint32_t DFT_SCALING_18432[5][7] = {{1,1,0,0,3,3,0},{1,1,0,0,3,3,0},{1,1,0,0,3,3,0},{1,1,0,0,3,3,0},{1,1,0,0,3,3,0}};
uint32_t DFT_SCALING_24576[5][7] = {{1,1,0,0,3,3,0},{1,1,0,0,3,3,0},{1,1,0,0,3,3,0},{1,1,0,0,3,3,0},{1,1,0,0,3,3,0}};
uint32_t DFT_SCALING_32768[5][7] = {{1,0,0,1,3,3,0},{1,0,0,1,3,3,0},{1,0,0,1,3,3,0},{1,0,0,1,3,3,0},{1,0,0,1,3,3,0}};
uint32_t DFT_SCALING_36864[5][7] = {{1,1,0,0,3,3,0},{1,1,0,0,3,3,0},{1,1,0,0,3,3,0},{1,1,0,0,3,3,0},{1,1,0,0,3,3,0}};
uint32_t DFT_SCALING_49152[5][7] = {{1,0,0,1,3,3,0},{1,0,0,1,3,3,0},{1,0,0,1,3,3,0},{1,0,0,1,3,3,0},{1,0,0,1,3,3,0}};
uint32_t DFT_SCALING_65536[5][7] = {{0,0,0,2,3,3,0},{0,0,0,2,3,3,0},{0,0,0,2,3,3,0},{0,0,0,2,3,3,0},{0,0,0,2,3,3,0}};
uint32_t DFT_SCALING_73728[5][8] = {{1,1,1,0,0,3,3,0},{1,1,1,0,0,3,3,0},{1,1,1,0,0,3,3,0},{1,1,1,0,0,3,3,0},{1,1,1,0,0,3,3,0}};
uint32_t DFT_SCALING_98304[5][8] = {{1,1,0,0,1,3,3,0},{1,1,0,0,1,3,3,0},{1,1,0,0,1,3,3,0},{1,1,0,0,1,3,3,0},{1,1,0,0,1,3,3,0}};

uint32_t IDFT_SCALING_128[2][2] = {{2,2},{1,3}};
uint32_t IDFT_SCALING_256[2][2] = {{2,2},{1,3}};
uint32_t IDFT_SCALING_512[2][3] = {{1,2,2},{1,1,3}};
uint32_t IDFT_SCALING_768[2][3] = {{1,2,2},{1,1,3}};
uint32_t IDFT_SCALING_1024[2][3] = {{4,1,0},{1,1,3}};
uint32_t IDFT_SCALING_1536[2][4] = {{1,1,1,3},{1,1,1,3}};
uint32_t IDFT_SCALING_2048[2][4] = {{3,2,1,0},{1,1,1,3}};
uint32_t IDFT_SCALING_3072[2][4] = {{1,1,1,3},{1,1,1,3}};
uint32_t IDFT_SCALING_4096[2][4] = {{3,2,1,0},{1,1,1,3}};
uint32_t IDFT_SCALING_6144[2][5] = {{1,1,0,3,2},{1,1,1,1,3}};
uint32_t IDFT_SCALING_8192[2][5] = {{1,0,0,3,3},{1,1,1,1,3}};
uint32_t IDFT_SCALING_9216[2][5] = {{1,0,0,3,3},{1,1,1,1,3}};
uint32_t IDFT_SCALING_12288[2][5] = {{1,0,0,3,3},{1,1,1,1,3}};
uint32_t IDFT_SCALING_16384[2][5] = {{0,0,1,3,3},{1,1,1,1,3}};
uint32_t IDFT_SCALING_18432[2][6] = {{1,1,0,0,3,3},{1,1,1,1,1,3}};
uint32_t IDFT_SCALING_24576[2][6] = {{1,1,0,0,3,3},{1,1,1,1,1,3}};
uint32_t IDFT_SCALING_32768[2][6] = {{1,0,0,1,3,3},{1,1,1,1,1,3}};
uint32_t IDFT_SCALING_36864[2][6] = {{1,1,0,0,3,3},{1,1,1,1,1,3}};
uint32_t IDFT_SCALING_49152[2][6] = {{1,0,0,1,3,3},{1,1,1,1,1,3}};
uint32_t IDFT_SCALING_65536[2][6] = {{0,0,0,2,3,3},{1,1,1,1,1,3}};
uint32_t IDFT_SCALING_73728[2][7] = {{1,1,1,0,0,3,3},{1,1,1,1,1,1,3}};
uint32_t IDFT_SCALING_98304[2][7] = {{1,1,0,0,1,3,3},{1,1,1,1,1,1,3}};

/* function description array, to be used when loading the dfts/idfts lib */
static loader_shlibfunc_t shlib_fdesc[2];
static char *arg[64] = {"phytest", "-O", "cmdlineonly::dbgl0"};
dftfunc_t dft;
idftfunc_t idft;
int load_dftslib(void)
{
  char *ptr = (char *)config_get_if();
  if (ptr == NULL) { // phy simulators, config module possibly not loaded
    uniqCfg = load_configmodule(3, (char **)arg, CONFIG_ENABLECMDLINEONLY);
    logInit();
  }
  shlib_fdesc[0].fname = "dft_implementation";
  shlib_fdesc[1].fname = "idft_implementation";
  int ret = load_module_shlib("dfts", shlib_fdesc, sizeof(shlib_fdesc) / sizeof(loader_shlibfunc_t), NULL);
  AssertFatal((ret >= 0), "Error loading dftsc decoder");
  dft = (dftfunc_t)shlib_fdesc[0].fptr;
  idft = (idftfunc_t)shlib_fdesc[1].fptr;
  return 0;
}
