#! /bin/bash

if [ "$#" -ne 2 ]; then
    exit 1
fi

CONT_VETH=$1
CONT_IP=$2

ip link set "${CONT_VETH}" up && \
ip addr add "${CONT_IP}" dev "${CONT_VETH}"

exit $?