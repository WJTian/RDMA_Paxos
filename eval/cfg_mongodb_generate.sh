#/bin/bash

config_file="nodes.local.cfg"
rm -rf $config_file

touch $config_file

echo "#configuration files for the replicated state machine node group

group_size = $1;

mgr_global_config = {
    rsm = 1;
    check_output = 0;
};

mgr_config =(
" >> $config_file

echo "    {
        ip_address = \"202.45.128.160\";
        port       = $3;
        db_name    = \"node_test_0\";
        time_stamp_log = 0;
        sys_log = 1;
        stat_log = 0;
        req_log = 0;
    }," >> $config_file

for id in $( seq 1 $[$1-1])
do
config_port=$[$3+$[$[$id-1]/$[$2-1]]]
ip_end=$[$[$id-1]%$[$2-1]+2]
echo "    {
        ip_address = \"10.22.1.$ip_end\";
        port       = $config_port;
        db_name    = \"node_test_$id\";
        time_stamp_log = 0;
        sys_log = 1;
        stat_log = 0;
        req_log = 0;" >> $config_file
if [ $id != $[$1-1] ]
then
echo "    }," >> $config_file
else
echo "    }" >> $config_file
fi
done

echo ");

zookeeper_config =(" >> $config_file

for id in $( seq 0 $[$1-1])
do
if  [ $id != $[$1-1] ]
then
echo "    {
        port       = 9001;
    }," >> $config_file
else
echo "    {
        port       = 9001;
    }" >> $config_file
fi
done

echo ");" >> $config_file

echo "consensus_config =(" >> $config_file

echo "    {
        ip_address = \"10.22.1.1\";
        port       = 8000;
        db_name    = \"node_test_0\";
        sys_log = 0;
        stat_log = 0;
    }," >> $config_file


for id in $( seq 1 $[ $1 - 1 ])
do
consensus_port=$[8000+$[$[$id-1]/$[$2-1]]]
ip_end=$[$[$id-1]%$[$2-1]+2]
echo "    {
        ip_address = \"10.22.1.$ip_end\";
        port       = $consensus_port;
        db_name    = \"node_test_$id\";
        sys_log = 0;
        stat_log = 0;" >> $config_file
if [ $id != $[$1-1] ]
then
echo "    }," >> $config_file
else
echo "    }" >> $config_file
fi
done

echo ");" >> $config_file

cp $config_file $RDMA_ROOT/RDMA/target

scp $config_file 10.22.1.2:$RDMA_ROOT/RDMA/target
scp $config_file 10.22.1.3:$RDMA_ROOT/RDMA/target
scp $config_file 10.22.1.4:$RDMA_ROOT/RDMA/target
scp $config_file 10.22.1.5:$RDMA_ROOT/RDMA/target
scp $config_file 10.22.1.6:$RDMA_ROOT/RDMA/target
scp $config_file 10.22.1.7:$RDMA_ROOT/RDMA/target
scp $config_file 10.22.1.8:$RDMA_ROOT/RDMA/target
scp $config_file 10.22.1.9:$RDMA_ROOT/RDMA/target
