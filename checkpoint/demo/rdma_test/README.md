**Mellanox OFED for Linux Installation**  
1. Download MLNX_OFED at http://www.mellanox.com/page/products_dyn?product_family=26&mtag=linux_sw_drivers.  
2. Login to the installation machine as root.  
3. Mount the ISO image on your machine. `mount -o ro,loop MLNX_OFED_LINUX-<ver>-<OS label>.iso /mnt`.  
4. Run the installation script. `/mnt/mlnxofedinstall --without-fw-update`.  
5.  Run the `/usr/bin/hca_self_test.ofed` utility to verify whether or not the InfiniBand link is up.
