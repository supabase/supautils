-- These bench tests are meant to exercise the statements that
-- supautils ProcessUtility_hook (supautils_hook) touches
-- TODO not fully complete (some details missing plus event triggers)

-- ROLE utility statements
CREATE ROLE bench_role_:client_id LOGIN;
CREATE ROLE bench_role_member_:client_id LOGIN;
ALTER ROLE bench_role_:client_id NOLOGIN NOINHERIT NOBYPASSRLS;
ALTER ROLE bench_role_:client_id SET search_path TO pg_catalog;
GRANT bench_role_:client_id TO bench_role_member_:client_id;
REVOKE bench_role_:client_id FROM bench_role_member_:client_id;
ALTER ROLE bench_role_:client_id RENAME TO bench_role_renamed_:client_id;
DROP ROLE bench_role_member_:client_id;
DROP ROLE bench_role_renamed_:client_id;

-- EXTENSION utility statements
CREATE SCHEMA bench_ext_schema_:client_id;
CREATE EXTENSION pg_trgm WITH SCHEMA bench_ext_schema_:client_id;
ALTER EXTENSION pg_trgm SET SCHEMA public;
COMMENT ON EXTENSION pg_trgm IS 'pgbench :client_id';
DROP EXTENSION pg_trgm;
DROP SCHEMA bench_ext_schema_:client_id;

-- FDW utility statements
CREATE EXTENSION postgres_fdw;
CREATE FOREIGN DATA WRAPPER bench_fdw_:client_id
  HANDLER postgres_fdw_handler
  VALIDATOR postgres_fdw_validator;
DROP FOREIGN DATA WRAPPER bench_fdw_:client_id;
DROP EXTENSION postgres_fdw;

-- PUBLICATION utility statements
CREATE TABLE public.pub_table (id int PRIMARY KEY);
CREATE PUBLICATION bench_publication_:client_id
  FOR TABLE public.pub_table;
ALTER PUBLICATION bench_publication_:client_id
  ADD TABLES IN SCHEMA public;
DROP PUBLICATION bench_publication_:client_id;
DROP TABLE public.pub_table;

-- RLS policies utility statements
CREATE TABLE bench_policy_table_:client_id (id int PRIMARY KEY, owner text);
ALTER TABLE bench_policy_table_:client_id ENABLE ROW LEVEL SECURITY;
CREATE POLICY bench_policy_:client_id
  ON bench_policy_table_:client_id
  FOR SELECT
  USING (owner = current_user);
ALTER POLICY bench_policy_:client_id
  ON bench_policy_table_:client_id
  USING (owner IS NOT NULL);
DROP POLICY bench_policy_:client_id ON bench_policy_table_:client_id;
DROP TABLE bench_policy_table_:client_id;

-- TRIGGER utility statements
CREATE TABLE bench_trigger_table_:client_id (id int);
CREATE FUNCTION bench_trigger_fn_:client_id()
RETURNS trigger
LANGUAGE plpgsql
AS $$
BEGIN
  RETURN NEW;
END;
$$;
CREATE TRIGGER bench_trigger_:client_id
  BEFORE INSERT ON bench_trigger_table_:client_id
  FOR EACH ROW
  EXECUTE FUNCTION bench_trigger_fn_:client_id();
DROP TRIGGER bench_trigger_:client_id ON bench_trigger_table_:client_id;
DROP FUNCTION bench_trigger_fn_:client_id();
DROP TABLE bench_trigger_table_:client_id;
