# RDMA_Paxos

This project combines RDMA (Remote Direct Memory Access) and Paxos.  

OS: Ubuntu 14.04.02 64bit.  
  
**Build php**  
  
1. Install depdendent libraries/tools:  
`sudo apt-get install libxml2-dev`  
  
2. ./apps/apache/mk  
php-cgi: install-php/bin/php-cgi  
  
**Install the dependencies for the program**  
Use ./RDMA/mk to download and install the dependencies for the Paxos program (those libraries will be installed in ./.local, and sources files will be kept in ./dep-lib)  
subdir.mk: `-I"$(ROOT_DIR)/../.local/include"`  
makefile: `-L"$(ROOT_DIR)/../.local/lib"`  
  
**Set env vars in ~/.bashrc.**  
`export LD_LIBRARY_PATH=/home/wangcheng/Downloads/RDMA_Paxos-master/RDMA/.local/lib:$LD_LIBRARY_PATH`


