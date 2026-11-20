CREATE FUNCTION stats_run_elevated(OUT numcalls bigint,
    OUT stats_reset timestamptz)
RETURNS record
AS 'MODULE_PATHNAME', 'stats_run_elevated'
LANGUAGE C STRICT PARALLEL UNSAFE;
