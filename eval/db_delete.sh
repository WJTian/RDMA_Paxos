#!/bin/bash

set -x

if [ -f ~/.bashrc ]; then
  source ~/.bashrc
fi

#ssh -f $LOGNAME@$(cat $RDMA_ROOT/apps/env/local_host) "sudo rm -rf ~/.db"
#ssh -f $LOGNAME@$(cat $RDMA_ROOT/apps/env/remote_host1) "sudo rm -rf ~/.db"
#ssh -f $LOGNAME@$(cat $RDMA_ROOT/apps/env/remote_host2) "sudo rm -rf ~/.db"

#ssh -f $LOGNAME@$(cat $RDMA_ROOT/apps/env/local_host) "sudo rm -rf ~/node*"
#ssh -f $LOGNAME@$(cat $RDMA_ROOT/apps/env/remote_host1) "sudo rm -rf ~/node*"
#ssh -f $LOGNAME@$(cat $RDMA_ROOT/apps/env/remote_host2) "sudo rm -rf ~/node*"

cat $RDMA_ROOT/apps/env/local_host | while read line
do
    ssh -f $LOGNAME@${line} "sudo rm -rf ~/.db"
    ssh -f $LOGNAME@${line} "sudo rm -rf ~/node*"
done

cat $RDMA_ROOT/apps/env/remote_hosts | while read line
do
    ssh -f $LOGNAME@${line} "sudo rm -rf ~/.db"
    ssh -f $LOGNAME@${line} "sudo rm -rf ~/node*"
done

