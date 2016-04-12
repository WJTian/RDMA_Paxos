#!/bin/sh

# before dump
REDIS_CLI=./apps/redis-cli

ps -elf | grep redis
echo "set key 1" | $REDIS_CLI 
echo "get key" | $REDIS_CLI
curl "http://10.22.1.1:12345/checkpoint?node_id=0&round_id=1"
sleep 3 
curl "http://10.22.1.1:12345/checkpoint?node_id=0&round_id=2"
sleep 3 
echo "checkpoint finished"

echo "set key 2" | $REDIS_CLI 
echo "set key 3" | $REDIS_CLI 
echo "set key 4" | $REDIS_CLI 
echo "get key" | $REDIS_CLI
ps -elf | grep redis


sleep 3
curl "http://10.22.1.1:12345/restore?node_id=0&round_id=5"
sleep 3 
ps -elf | grep redis
sleep 3
echo "restore finished"
echo "get key" | $REDIS_CLI

