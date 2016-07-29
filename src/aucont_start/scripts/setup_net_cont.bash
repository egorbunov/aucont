#! /bin/bash

if [ "$#" -ne 3 ]; then
    exit 1
fi

CONT_VETH=$1
CONT_IP=$2
HOST_IP=$3

ip addr add "${CONT_IP}/24" dev "${CONT_VETH}" && \
ip link set "${CONT_VETH}" up && \
ip route add default via "${HOST_IP}" && \
ip link set lo up

exit $?