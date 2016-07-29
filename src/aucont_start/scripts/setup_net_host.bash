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
sudo ip addr add "${HOST_IP}/24" dev "${HOST_VETH}" && \
sudo ip link set "${HOST_VETH}" up && \
sudo sysctl net.ipv4.conf.all.forwarding=1 && \
sudo iptables -P FORWARD DROP && \
sudo iptables -F FORWARD && \
sudo iptables -t nat -F && \
sudo iptables -t nat -A POSTROUTING -s "${HOST_IP}/24" -o eth0 -j MASQUERADE && \
sudo iptables -A FORWARD -i eth0 -o "${HOST_VETH}" -j ACCEPT && \
sudo iptables -A FORWARD -o eth0 -i "${HOST_VETH}" -j ACCEPT

exit $?