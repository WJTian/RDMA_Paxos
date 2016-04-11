#!/bin/sh 

set -x
#start redis firstly
sh ./stop.sh
ps -elf | grep redis
app_cmd="./apps/redis-server ./apps/redis.conf"
$app_cmd
sleep 1
app_pid=`cat ./redis.pid`
ls -la /proc/$app_pid/fd
#python guard.py node_id pid rdma.cfg
cfg_path=/home/jingyu/code/rdma_master/RDMA_Paxos/RDMA/target/nodes.local.cfg
self_id=0

sudo python guard.py $self_id $app_pid $cfg_path
