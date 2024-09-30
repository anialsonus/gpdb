--
-- Test the query command identification
--
create extension if not exists gp_inject_fault;

-- start_ignore
drop function if exists sirv_function();
drop function if exists not_inlineable_sql_func(i int);
drop table if exists test_data1;
drop table if exists test_data2;
drop table if exists t;
drop table if exists t1;
drop table if exists t2;
-- end_ignore

set client_min_messages = notice;
select gp_inject_fault('all', 'reset', dbid) from gp_segment_configuration;

create or replace function sirv_function() returns text as $$
declare
    result1 text;
    result2 text;
    result3 text;
begin
    create table test_data1 (x int, y int) distributed by (x);
    create table test_data2 (x int, y varchar) distributed by(x);

    execute 'insert into test_data1 values (1,1)';
    execute 'insert into test_data1 values (1,2)';

    execute 'insert into test_data2 values (1, ''one'')';
    execute 'insert into test_data2 values (1, ''ONE'')';

    execute 'select case when count(*)>0 then ''PASS'' else ''FAIL'' end from test_data1' into result1;
    execute 'select case when count(*)>0 then ''PASS'' else ''FAIL'' end from test_data2' into result2;

    execute 'drop table test_data1';
    execute 'drop table test_data2';

    if (result1 = 'PASS')  and  (result2 = 'PASS') then
        result3 = 'PASS';
    else
        result3 = 'FAIL';
    end if;
    return result3;
end $$ language plpgsql volatile;

\c

select gp_inject_fault_infinite('track_query_command_id', 'skip', dbid) from gp_segment_configuration
where role = 'p' and content = -1;

select sirv_function();

-- Test that the query command id is correct after execution of queries in the InitPlan
create table t as select (select sirv_function()) as res distributed by (res);

-- Test a simple query
select * from t;

drop table t;

-- Test a cursor
begin;
declare cur1 cursor for select sirv_function() as res;
fetch 1 from cur1;
fetch all from cur1;
commit;

-- Test two cursors
begin;
declare cur1_a cursor for select sirv_function() as res;
fetch 1 from cur1_a;
declare cur2_b cursor for select sirv_function() as res;
fetch 2 from cur2_b;
fetch all from cur2_b;
fetch all from cur1_a;
commit;

-- Test partitioned tables
create table t(i int) distributed by (i)
partition by range (i) (start (1) end (10) every (1), default partition extra);

alter table t rename to t1;
alter table t1 rename to t2;

drop table t2;

-- Test a function written in sql language, that optimizers cannot inline
create or replace function not_inlineable_sql_func(i int) returns int 
immutable
security definer
as $$
select case when i > 5 then 1 else 0 end;
$$ language sql;

select not_inlineable_sql_func(i) from generate_series(1, 10)i;

select gp_inject_fault_infinite('track_query_command_id', 'reset', dbid) from gp_segment_configuration
where role = 'p' and content = -1;

-- Test the query command ids dispatched to segments
-- start_matchsubs
-- m/select pg_catalog.pg_relation_size\([0-9]+, \'.+\'\)/
-- s/select pg_catalog.pg_relation_size\([0-9]+, \'.+\'\)/select pg_catalog.pg_relation_size\(\)/
-- m/select pg_catalog.gp_acquire_sample_rows\([0-9]+, [0-9]+, \'.+'\)/
-- s/select pg_catalog.gp_acquire_sample_rows\([0-9]+, [0-9]+, \'.+'\)/select pg_catalog.gp_acquire_sample_rows\(\)/
-- m/FROM pg_aoseg.pg_aoseg_[0-9]+/
-- s/FROM pg_aoseg.pg_aoseg_[0-9]+/FROM pg_aoseg.pg_aoseg_OID/
-- end_matchsubs
select gp_inject_fault_infinite('track_query_command_id_at_start', 'skip', dbid) from gp_segment_configuration;

create table t as select 1;
drop table t;

create table t (i int, j text) with (appendonly = true) distributed by (i);
insert into t select i, (i + 1)::text from generate_series(1, 100) i;
vacuum analyze t;
drop table t;

select gp_inject_fault_infinite('track_query_command_id_at_start', 'reset', dbid) from gp_segment_configuration;

drop function sirv_function();
drop function not_inlineable_sql_func(i int);
reset client_min_messages;
