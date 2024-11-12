-- start_matchsubs
-- m/ERROR:  Track for database \d+ is being acquired in other transaction/
-- s/\d+/XXX/g
-- end_matchsubs
-- Test concurrent track acquisition.
1: CREATE EXTENSION IF NOT EXISTS arenadata_toolkit;
1: SELECT arenadata_toolkit.tracking_register_db();
1: SELECT arenadata_toolkit.tracking_trigger_initial_snapshot();
1: BEGIN;
1: SELECT tt.segid, count(*) FROM arenadata_toolkit.tables_track tt GROUP BY tt.segid;

2: SELECT tt.segid, count(*) FROM arenadata_toolkit.tables_track tt GROUP BY tt.segid;

1: ROLLBACK;

2: SELECT tt.segid, count(*) FROM arenadata_toolkit.tables_track tt GROUP BY tt.segid;

1: SELECT arenadata_toolkit.tracking_unregister_db();
1q:
2q:

!\retcode gpconfig -r shared_preload_libraries;
!\retcode gpconfig -r arenadata_toolkit.tracking_worker_naptime_sec;
!\retcode gpstop -raq -M fast;