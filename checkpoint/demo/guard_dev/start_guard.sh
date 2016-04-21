#!/bin/sh

set -x

aim_name="redis-server"

if [ ! -z "$1" ]; then
	aim_name=$1
fi

self_id=0

if [ ! -z "$2" ]; then
	self_id=$2
fi

cfg_path=../../../RDMA/target/nodes.local.cfg

# remove socket file
SOCK_FILE=/tmp/guard.sock
sudo rm -rf $SOCK_FILE

python ./guard.py $self_id $aim_name $cfg_path
