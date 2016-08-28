#!/bin/sh

# Author: jingyu
# Date: April 30, 2016
# Description: all eval.py for every cfg
# usage: bash ./auto_test_all.sh 2>&1 |  tee ./auto_test_all.run.log
BLACK_LIST="zookeeper.cfg guard-ck.cfg calvin.cfg mysql-output.cfg mongodb-output.cfg"
InBlackList(){
	for item in $BLACK_LIST; do
		if [ $1 = $item ]; then
			return 1
		fi
	done
	return 0
}

echo "`date` Auto Test All Starts"

# get all avaliable cfg files
cfg_list=`ls *mediatomb-output.cfg`
for cfg in $cfg_list ; do
	echo "`date` Handle CFG: $cfg starts"
	InBlackList $cfg 
	retcode=$?	
	if [ $retcode = 1 ]; then
		echo "$cfg is in the black list, skip it."
	else
		echo "`date` start run: python eval -f $cfg"  >>  ~/result.txt
		python eval_multi.py -f $cfg
		echo "`date` finished: python eval -f $cfg"
                sed -i 's/;/ /g' ~/node-0-consensus-sys.log
                awk '{ sum += $1; n++ } END { if (n > 0) print sum / n; }' ~/node-0-consensus-sys.log >> ~/result.txt
                awk '{ sum += $2; n++ } END { if (n > 0) print sum / n; }' ~/node-0-consensus-sys.log >> ~/result.txt
	fi
	echo "`date` Handle CFG: $cfg ends"
done

echo "`date` Auto Test All ends"
