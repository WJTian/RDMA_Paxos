#!/bin/sh 

set -x
#start redis firstly
sh ./stop.sh
sleep 1
ps -elf | grep redis

self_id=0
cfg_path=./nodes.local.cfg
#app_cmd="env LD_LIBRARY_PATH=. LD_PRELOAD= node_id=$self_id cfg_path=$cfg_path ./apps/redis-server ./apps/redis.conf "
app_cmd="env LD_LIBRARY_PATH=. LD_PRELOAD=./interpose.so node_id=$self_id cfg_path=$cfg_path ./apps/redis-server ./apps/redis.conf "
$app_cmd
sleep 3 
app_pid=`cat ./redis.pid`
ls -la /proc/$app_pid/fd

lsof -p $app_pid
#python guard.py node_id pid rdma.cfg

python guard.py $self_id $app_pid $cfg_path
