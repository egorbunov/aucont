#! /bin/bash

if [ "$#" -ne 2 ]; then
    exit 1 # wrong number of arguments
fi

CONTAINER_PID=$1
CGROUP_HIERARCHY_DIR=$2
CONT_CPU_CGROUP_NAME=$(cat /proc/${CONTAINER_PID}/cgroup | \
                       grep -w cpu | \
                       awk -F ":" '{print $3}' | \
                       sed "s/\/\(.*\)/\\1/")
echo ${CONT_CPU_CGROUP_NAME}
CGROUP_PATH=${CGROUP_HIERARCHY_DIR}/${CONT_CPU_CGROUP_NAME}/tasks
echo ${CGROUP_PATH}
echo ${CONTAINER_PID} > ${CGROUP_PATH}
exit $?

