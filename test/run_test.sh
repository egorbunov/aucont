#!/bin/bash

rm -rf ./rootfs
tar -xzf rootfs.tar.gz

# if [ ! -d "./rootfs" ]; then
#     tar -xzvf rootfs.tar.gz
# fi

AUCONT_TEST_DOCKER_CONTAINER_IMAGE_NAME=${1:-egorbunov/aucont16}
AUCONT_TOOLS_BIN_DIR_PATH=${2:-`pwd`/../}
AUCONT_TEST_SCRIPTS_DIR_PATH=${3:-`pwd`/scripts/}
AUCONT_TEST_CONT_ROOTFS_DIR_PATH=${4:-`pwd`/rootfs/}

sudo docker build -t "${AUCONT_TEST_DOCKER_CONTAINER_IMAGE_NAME}" ./container


sudo docker run -ti \
    -v ${AUCONT_TOOLS_BIN_DIR_PATH}:'/test/aucont' \
    -v ${AUCONT_TEST_SCRIPTS_DIR_PATH}:'/test/scripts' \
    -v ${AUCONT_TEST_CONT_ROOTFS_DIR_PATH}:'/test/rootfs' \
    --net=host --pid=host --privileged=true \
    ${AUCONT_TEST_DOCKER_CONTAINER_IMAGE_NAME}
