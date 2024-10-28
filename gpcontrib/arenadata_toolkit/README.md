## arenadata_toolkit - database objects tracking extenstion for GPDB

`arenadata_toolkit` starting from version 1.7 represents a GPDB extension that efficiently tracks file system changes (extend, truncate, create, unlink operations) using space-efficient Bloom filters stored in shared memory. This extension is particularly useful for monitoring and maintaining database files sizes across a distributed environment.

The main purpose of this code is achieving fast database size calculation and tracking file changes at relation
level. The extension implements a probabilistic tracking system using Bloom filters to monitor file changes across Greenplum segments. It utilizes shared memory for state management and employs a background workers to maintain consistency. 

#### Configuring GPDB and extension usage
Since extension uses shared memory, configuration on all GPDB segments must be changed by setting
```shell script
gpconfig -c shared_preload_libraries -v 'arenadata_toolkit'
```
Extension may track restricted number of databases. The maximum number of them is defined by GUC
| arenadata_toolkit.tracking_db_track_count | Need restart |Possible values [1, 1000]; Default 5|
|--|--|--|
For each tracked database there allocated a Bloom filter in shared memory. The size of each filter is controlled via
| arenadata_toolkit.tracking_bloom_size | Need restart |Possible values (bytes) [64, 128000000] Default 1048576|
|--|--|--|
The specific database can be binded to unoccupied filter with function
```shell script
psql -d my_db -c select arenadata_toolkit.tracking_register_db()
or
psql -c select arenadata_toolkit.tracking_register_db(12345)
```
After registering each relation file change withing the database will be noted in Bloom filter.
Using Bloom filter allows us to calculate the sizes of only relations whose relfilenode is present in the filter.
The current size snapshot can be taken via view:
```
select * from arenadata_toolkit.tables_track;
```
In order to get the snapshot of all database relations you should call in the database of interest
```
arenadata_toolkit.tracking_trigger_initial_snapshot();
```

#### Choosing optimal Bloom size

Choosing the optimal Bloom filter size is crucial for balancing memory usage and accuracy.
First of all, when choosing the filter size, you should take into account your system resources, because bloom filters are allocated in shared memory for each segment, and too wide structures (tracking_db_track_count * tracking_bloom_size) could decrease overall perfomance.

Next, choose the filter size satisfying your performance goals:
- Define false positive tolerance, p. Since Bloom filter is probabilistic data structure there is a probability to calculate the size of relation, which has not been modified. And the smaller filter is, the more often this occurs.
- Memory constraints 
- Query patterns, if queries are mostly reading then huge sizes are unecessary.

If you will estimate number of objects in your database, you can calculate theoretical size:
$$m = -\frac{n \ln p}{(\ln 2)^2}$$
- n = estimated number of elements
- p = target false positive rate
- m = filter size in bits

Quick Reference Table

 Deployment Size | Files      | Target FPR | Recommended Size |
|----------------|------------|------------|------------------|
| Small          | < 100K     | 1%        | 1 MB            |
| Medium         | 100K - 1M  | 1%        | 8 MB            |
| Large          | > 1M       | 1%        | 32 MB           |
| Enterprise     | > 10M      | 0.1%      | 128 MB          |