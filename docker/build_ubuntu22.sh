#!/bin/bash

function usage() {
  echo "usage: $0 [options]"
  echo "  -c <loc> set ccache location to <loc>; use /tmp to disable ccache [default: ~/.cache/ccache]"
  echo "  -f       force build base image if exists"
  echo "  -h       this help"
  echo "  -i       build incrementally from existing ran-build image, if exists"
  echo "  -l <dir> use <dir> for saving logs [default: ./logs/]"
  echo "  -r <reg> prepend <reg> to target images (<reg> should end on /)"
  echo "  -t <tag> use <tag> as image tag for target images"
  echo "  -T <tag> use <tag> as image tag for base/build images"
  exit 0
}
function die() {
  echo $@
  exit 1
}
function get_log_path() {
  local pattern=$1 file=$2
  grep "${pattern}" "${file}" | sed 's/^#[0-9a-zA-Z. ]\+: //'
}
function copy_out_log() {
  local image=$1 file=$2 dest=$3
  docker run --name copy-out -d ${image} /bin/true
  docker cp copy-out:${file} ${dest}
  docker rm -f copy-out
}

export OAI_BASE_TAG=latest
export OAI_TAG=latest
export CCACHE_LOCATION=~/.cache/ccache/
LOG_DIR=./logs
CONFIG=docker-compose.ubuntu.yml
unset FORCE_BUILD_BASE
unset BUILD_INCR
export UID_GID=$(id -u):$(id -g) # recommended to set manually

while getopts ":c:fhil:r:t:T:" opt; do
  case ${opt} in
    c) export CCACHE_LOCATION=${OPTARG};;
    h) usage;;
    i) BUILD_INCR=0;;
    l) LOG_DIR=${OPTARG};;
    r) [[ ${OPTARG} =~ [0-9a-zA-Z.-_/]+/ ]] || die "invalid registry ${OPTARG}";
       export REGISTRY=${OPTARG};;
    t) export OAI_TAG=${OPTARG};;
    T) export OAI_BASE_TAG=${OPTARG};;
    f) FORCE_BUILD_BASE=0;;
    ?) die "unknown parameter -${OPTARG}";;
  esac
done
[ ${OPTIND} -ge $# ] || die "unknown non-option: ${!OPTIND}"

echo "OAI_TAG=${OAI_TAG}"
echo "OAI_BASE_TAG=${OAI_BASE_TAG}"
echo "CCACHE_LOCATION=${CCACHE_LOCATION}"
echo "REGISTRY=${REGISTRY}"

trap "docker compose -f ${CONFIG} down -t1" SIGINT SIGTERM

set -eo pipefail

rm -rf ${LOG_DIR}

# check if we are asked to force the build of the base image, in which case we
# remove the image to force the rebuild, and add --no-cache
if [ -v FORCE_BUILD_BASE ]; then
  docker rmi ran-base:${OAI_BASE_TAG} || true
  no_cache="--no-cache"
fi
if [ -z "$(docker images -q ran-base:${OAI_BASE_TAG})" ]; then
  # build new base image, putting the build output into ran-base.log, and
  # ASN1/UHD logs into ran-base/<comp>
  # do not cache anything to be sure we rebuild it
  mkdir -p ${LOG_DIR}/ran-base/
  docker compose -f ${CONFIG} --progress=plain build ${no_cache} ran-base | tee ${LOG_DIR}/ran-base.log
  path=$(get_log_path "log file for ASN1 installation" ${LOG_DIR}/ran-base.log)
  [ -n "${path}" ] && copy_out_log ran-base:${OAI_BASE_TAG} $path ${LOG_DIR}/ran-base/
  path=$(get_log_path "log file for UHD driver installation" ${LOG_DIR}/ran-base.log)
  [ -n "${path}" ] && copy_out_log ran-base:${OAI_BASE_TAG} $path ${LOG_DIR}/ran-base/
else
  echo "reusing previous ran-base:${OAI_BASE_TAG} image"
fi

# check if we are asked to build incrementally, and if a ran-build image exists
# in which case we use an existing ran-build as a basis to build the new one
mkdir -p ${LOG_DIR}/ran-build/
if [ -v BUILD_INCR ]; then
  # if no image yet, create initial one
  if [ -z  "$(docker images -q ran-build:${OAI_BASE_TAG:-latest})" ]; then
    docker compose -f ${CONFIG} --progress=plain build ran-build-incr-init
  fi
  docker compose -f ${CONFIG} --progress=plain build ran-build-incr
  path="/oai-ran/cmake_targets/log/all.txt"
  copy_out_log ran-build:${OAI_BASE_TAG} ${path} ${LOG_DIR}/ran-base/
else
  die "this does not work here, docker file changes in this commit need to be reverted"
  export BUILD_OPTS="-c" # start with clean directory
  docker compose -f ${CONFIG} up ran-build --pull=never --no-log-prefix --exit-code-from ran-build --abort-on-container-exit | tee ${LOG_DIR}/ran-build.log
  docker compose -f ${CONFIG} cp ran-build:/oai-ran/cmake_targets/log/all.txt ${LOG_DIR}/ran-build/
fi

docker compose -f ${CONFIG} --progress=plain build \
  oai-gnb oai-enb oai-nr-ue oai-lte-ue oai-lte-ru oai-nr-cuup
