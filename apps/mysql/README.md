command to run:  
`sysbench --mysql-user=root --test=oltp --oltp-table-size=2000000 --oltp-table-name=sbtest --mysql-table-engine=InnoDB --mysql-engine-trx=yes --mysql-db=sysbench_db prepare` 

`sudo ./run`  
  
wangcheng@wangcheng-OptiPlex-9020:~$ `sysbench --mysql-host=202.45.128.160 --mysql-user=root --mysql-port=3306 --num-threads=1 --max-requests=100 --test=oltp --oltp-table-size=2000000 --oltp-table-name=sbtest --mysql-engine-trx=yes --mysql-db=sysbench_db --oltp-test-mode=complex --mysql-table-engine=InnoDB --oltp-index-updates=200 --oltp-non-index-updates=200 run`
sysbench 0.4.12:  multi-threaded system evaluation benchmark

No DB drivers specified, using mysql
Running the test with following options:
Number of threads: 1

Doing OLTP test.
Running mixed OLTP test
Using Special distribution (12 iterations,  1 pct of values are returned in 75 pct cases)
Using "BEGIN" for starting transactions
Using auto_inc on the id column
Maximum number of requests for OLTP test is limited to 100
Threads started!
Done.

OLTP test statistics:
    queries performed:
        read:                            1400
        write:                           40300
        other:                           200
        total:                           41900
    transactions:                        100    (3.10 per sec.)
    deadlocks:                           0      (0.00 per sec.)
    read/write requests:                 41700  (1292.88 per sec.)
    other operations:                    200    (6.20 per sec.)

Test execution summary:
    total time:                          32.2535s
    total number of events:              100
    total time taken by event execution: 32.2428
    per-request statistics:
         min:                                280.91ms
         avg:                                322.43ms
         max:                                714.29ms
         approx.  95 percentile:             348.13ms

Threads fairness:
    events (avg/stddev):           100.0000/0.00
    execution time (avg/stddev):   32.2428/0.00

