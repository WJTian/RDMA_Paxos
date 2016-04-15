#!/bin/sh

# before dump
REDIS_CLI="./apps/redis-cli -h 10.22.1.1 -p 26379"

SERVICE_IP="10.22.1.1"
CK_NODE="1"  # 10.22.1.2
RE_NODE="2"  # 10.22.1.3

ps -elf | grep redis
echo "set key 1" | $REDIS_CLI 
echo "get key" | $REDIS_CLI

#curl "http://$SERVICE_IP:12345/checkpoint?node_id=$CK_NODE&round_id=1"
echo "checkpoint $CK_NODE 1\n" | nc -U /tmp/guard.sock
sleep 3 
#curl "http://$SERVICE_IP:12345/checkpoint?node_id=$CK_NODE&round_id=2"
echo "checkpoint $CK_NODE 2\n" | nc -U /tmp/guard.sock
sleep 3 
echo "checkpoint finished"

echo "set key 2" | $REDIS_CLI 
echo "set key 3" | $REDIS_CLI 
echo "set key 4" | $REDIS_CLI 
echo "get key" | $REDIS_CLI
ps -elf | grep redis

sleep 3
#curl "http://$SERVICE_IP:12345/restore?node_id=$RE_NODE&round_id=5"
echo "restore $RE_NODE 5\n" | nc -U /tmp/guard.sock
sleep 3 
ps -elf | grep redis
sleep 3
echo "restore finished"
echo "get key" | $REDIS_CLI

echo "get key" | ./apps/redis-cli -h 10.22.1.3 -p 26379
