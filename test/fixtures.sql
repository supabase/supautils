-- reserved roles
create role supabase_storage_admin noinherit;
create role anon noinherit;

-- login role
create user nosuper createrole nosuperuser;

-- other roles
create role fake noinherit;

-- pg_regress(invoked on make installcheck) uses this database
-- we create it ourselves and make pg_regress use it(see --use-existing on the Makefile)
-- because otherwise pg_regress creates it and when doing so it does an
-- `ALTER DATABASE "contrib_regression" SET lc_messages TO 'C'`
-- and that requires a SUPERUSER connection(which we don't want for testing the extension).
-- Otherwise `make installcheck` will failwith `ERROR:  permission denied to set parameter "lc_messages"`
create database contrib_regression;
