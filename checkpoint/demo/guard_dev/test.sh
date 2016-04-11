#!/bin/sh

# before dump
REDIS_CLI=./apps/redis-cli

echo "set a b" | $REDIS_CLI 
echo "get a" | $REDIS_CLI
curl -vvv "http://10.22.1.1:12345/checkpoint?node_id=0&round_id=1"
sleep 5
ps -elf | grep redis
sleep 3
curl -vvv "http://10.22.1.1:12345/restore?node_id=0&round_id=1"
ps -elf | grep redis
echo "get a" | $REDIS_CLI

