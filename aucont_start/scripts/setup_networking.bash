#! /bin/bash

if [ ! -n "$1" ]; then
    exit 1 # exit error: no net namespace name specified
fi

sudo ip netns add "$1"
