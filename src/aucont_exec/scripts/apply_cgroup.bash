#! /bin/bash
CONTAINER_PID=$1
CPU_CGROUP_PATH=$(cat /proc/${CONTAINER_PID}/cgroup | grep -w cpu | awk -F ":" '{print $3}')

