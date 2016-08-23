# RDMA_Paxos

This project combines RDMA (Remote Direct Memory Access) and Paxos. When cluster size varies from 3 to 17 nodes, the consensus latency increases merely by 16%~45%.

OS: Ubuntu 14.04.02 64bit.  
  
## How to run
### Install the dependencies for the program
Use $RDMA_ROOT/RDMA/mk to download and install the dependencies for the program (those libraries will be installed in $RDMA_ROOT/RDMA/.local, and sources files will be kept in $RDMA_ROOT/RDMA/dep-lib).
### Install the applications
We have prepared all the Makefiles for you in each application's directory.
### Run the evaluation framework
For example, to run Redis hooked by Falcon, just go to $RDMA_ROOT/eval and run `python eval.py -f redis-output.cfg`. After that, you can collect the results by `cd current`.

