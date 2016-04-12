#!/bin/sh

# before dump
REDIS_CLI=./apps/redis-cli

echo "set key 1" | $REDIS_CLI 
echo "get key" | $REDIS_CLI
curl -vvv "http://10.22.1.1:12345/checkpoint?node_id=0&round_id=1"
sleep 5
echo "checkpoint finished"

echo "set key 2" | $REDIS_CLI 
echo "set key 3" | $REDIS_CLI 
echo "set key 4" | $REDIS_CLI 
echo "get key" | $REDIS_CLI
ps -elf | grep redis


sleep 3
curl -vvv "http://10.22.1.1:12345/restore?node_id=0&round_id=1"
sleep 1
ps -elf | grep redis
sleep 1

echo "restore finished"
echo "get key" | $REDIS_CLI

