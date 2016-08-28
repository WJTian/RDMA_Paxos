#!/bin/bash

#clamav modify

sed -i "s/SERVER_COUNT=$1/SERVER_COUNT=$2/g" clamav-output.cfg
sed -i "s/SERVER_COUNT=$1/SERVER_COUNT=$2/g" redis-output.cfg
sed -i "s/SERVER_COUNT=$1/SERVER_COUNT=$2/g" memcached-output.cfg
sed -i "s/SERVER_COUNT=$1/SERVER_COUNT=$2/g" mediatomb-output.cfg
sed -i "s/SERVER_COUNT=$1/SERVER_COUNT=$2/g" ssdb-output.cfg
sed -i "s/SERVER_COUNT=$1/SERVER_COUNT=$2/g" ldap-output.cfg
