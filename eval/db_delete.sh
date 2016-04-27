#!/bin/bash

if [ -f ~/.bashrc ]; then
  source ~/.bashrc
fi

ssh -f $LOGNAME@$(cat $RDMA_ROOT/apps/env/local_host) "rm -rf $RDMA_ROOT/eval/.db"
ssh -f $LOGNAME@$(cat $RDMA_ROOT/apps/env/remote_host1) "rm -rf $RDMA_ROOT/eval/.db"
ssh -f $LOGNAME@$(cat $RDMA_ROOT/apps/env/remote_host2) "rm -rf $RDMA_ROOT/eval/.db"
