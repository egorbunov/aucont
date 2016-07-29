#! /bin/bash

if [ "$#" -ne 4 ]; then
    exit 1
fi

CONT_PID=$1
HOST_VETH=$2
CONT_VETH=$3
HOST_IP=$4

sudo ip link add "${HOST_VETH}" type veth peer name "${CONT_VETH}" && \
sudo ip link set "${CONT_VETH}" netns "${CONT_PID}" && \
sudo ip addr add "${HOST_IP}" dev "${HOST_VETH}" && \
sudo ip link set "${HOST_VETH}" up

exit $?