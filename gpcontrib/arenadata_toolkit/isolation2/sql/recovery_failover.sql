-- This test triggers failover of content 1 and checks
-- the correct tracking state behaviour after recovery
!\retcode gpconfig -c shared_preload_libraries -v 'arenadata_toolkit';
!\retcode gpconfig -c gp_fts_probe_retries -v 2 --masteronly;
-- Allow extra time for mirror promotion to complete recovery
!\retcode gpconfig -c gp_gang_creation_retry_count -v 120 --skipvalidation --masteronly;
!\retcode gpconfig -c gp_gang_creation_retry_timer -v 1000 --skipvalidation --masteronly;
!\retcode gpconfig -c arenadata_toolkit.tracking_worker_naptime_sec -v '5';
!\retcode gpstop -raq -M fast;

CREATE EXTENSION arenadata_toolkit;

SELECT pg_sleep(current_setting('arenadata_toolkit.tracking_worker_naptime_sec')::int);
SELECT arenadata_toolkit.tracking_register_db();
SELECT arenadata_toolkit.tracking_trigger_initial_snapshot();
SELECT tt.segid, count(*) FROM arenadata_toolkit.tables_track tt GROUP BY tt.segid;

include: helpers/server_helpers.sql;

-- Helper functions
CREATE OR REPLACE FUNCTION tracking_is_segment_initialized_master() RETURNS TABLE(segindex INT, is_initialized BOOL) LANGUAGE SQL EXECUTE ON MASTER AS $$ SELECT segindex, is_initialized FROM arenadata_toolkit.tracking_is_segment_initialized(); $$;

CREATE OR REPLACE FUNCTION tracking_is_segment_initialized_segments() RETURNS TABLE(segindex INT, is_initialized BOOL) LANGUAGE SQL EXECUTE ON ALL SEGMENTS AS $$ SELECT segindex, is_initialized FROM arenadata_toolkit.tracking_is_segment_initialized(); $$;

CREATE or REPLACE FUNCTION wait_until_segments_are_down(num_segs int)
RETURNS bool AS
$$
declare
retries int; /* in func */
begin /* in func */
  retries := 1200; /* in func */
  loop /* in func */
    if (select count(*) = num_segs from gp_segment_configuration where status = 'd') then /* in func */
      return true; /* in func */
    end if; /* in func */
    if retries <= 0 then /* in func */
      return false; /* in func */
    end if; /* in func */
    perform pg_sleep(0.1); /* in func */
    retries := retries - 1; /* in func */
  end loop; /* in func */
end; /* in func */
$$ language plpgsql;

-- no segment down.
select count(*) from gp_segment_configuration where status = 'd';

select pg_ctl((select datadir from gp_segment_configuration c
where c.role='p' and c.content=1), 'stop');

select wait_until_segments_are_down(1);

SELECT * FROM tracking_is_segment_initialized_master()
UNION ALL
SELECT * FROM tracking_is_segment_initialized_segments();

-- Track acquisition should retrurn full snapshot from promoted mirror since
-- initial snapshot is activated on recovery by deafult.
SELECT tt.segid, count(*) FROM arenadata_toolkit.tables_track tt GROUP BY tt.segid;

-- fully recover the failed primary as new mirror
!\retcode gprecoverseg -aF --no-progress;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

!\retcode gprecoverseg -ar;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

-- verify no segment is down after recovery
select count(*) from gp_segment_configuration where status = 'd';

-- Track should be returned only from recovered segment since
-- initial snapshot is activated on recovery by deafult.
SELECT tt.segid, count(*) FROM arenadata_toolkit.tables_track tt GROUP BY tt.segid;
SELECT arenadata_toolkit.tracking_unregister_db();

!\retcode gpconfig -r gp_fts_probe_retries --masteronly;
!\retcode gpconfig -r gp_gang_creation_retry_count --skipvalidation --masteronly;
!\retcode gpconfig -r gp_gang_creation_retry_timer --skipvalidation --masteronly;
!\retcode gpstop -u;