#! /bin/bash

if [ "$#" -ne 4 ]; then
    exit 1 # wrong number of arguments
fi

CPU_PERC=$1
CONT_PID=$2
CGROUP_HIERARCHY_DIR=$3
CGROUP_NAME=$4
CPU_CGROUP_DIR=${CGROUP_HIERARCHY_DIR}/${CGROUP_NAME}

# cheking if there already exists cgroup hierarchy with cpu subsystem attached
CPU_MOUNT_OPTS=$(mount \
          | grep -w "cgroup" | grep -w "cpu" | grep -o "(.*)" \
          | head -n 1 \
          | sed "s/(\([^()]\+\))/\\1/")

if [ -z "$CPU_MOUNT_OPTS" ]; then
    CPU_MOUNT_OPTS=cpu
fi

# mounting cgroup hierarchy if needed
if [ -z "$(mount | grep "$CGROUP_HIERARCHY_DIR")" ]; then
    mkdir -p "$CGROUP_HIERARCHY_DIR" && \
    mount -t cgroup -o"$CPU_MOUNT_OPTS" aucont_cgrouph "$CGROUP_HIERARCHY_DIR"
    if [ "$?" -ne "0" ]; then
        exit 2 # error mounting
    fi
fi

# creating cgroup for given cpu percentage if needed
MAX_PERIOD=1000000
mkdir -p "$CPU_CGROUP_DIR" && \
echo $MAX_PERIOD > ${CPU_CGROUP_DIR}/cpu.cfs_period_us && \
echo $(($CPU_PERC * $MAX_PERIOD / 100)) > ${CPU_CGROUP_DIR}/cpu.cfs_quota_us && \
echo $CONT_PID > ${CPU_CGROUP_DIR}/tasks

exit $?