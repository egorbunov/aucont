#! /bin/bash

if [ "$#" -ne 3 ]; then
    exit 1 # wrong number of arguments
fi

CONTAINER_PID=$1
SLAVE_PROCESS_PID=$2
CGROUP_HIERARCHY_DIR=$3
CONT_CPU_CGROUP_NAME=$(cat /proc/${CONTAINER_PID}/cgroup | \
                       grep -w cpu | \
                       awk -F ":" '{print $3}' | \
                       sed "s/\/\(.*\)/\\1/")
echo ${CONT_CPU_CGROUP_NAME}
CGROUP_PATH=${CGROUP_HIERARCHY_DIR}/${CONT_CPU_CGROUP_NAME}/tasks
echo $$
echo $CONTAINER_PID
echo $SLAVE_PROCESS_PID
echo ${CGROUP_PATH}
cat ${CGROUP_PATH}
sleep 30
echo ${SLAVE_PROCESS_PID} >> ${CGROUP_PATH}
exit $?

