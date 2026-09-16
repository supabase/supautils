set role extensions_role;
\echo

-- an error in a before-create script must not leave the session elevated
-- or stop later statements from elevating
create extension ltree;
select current_user;
create extension pageinspect;
drop extension pageinspect;
\echo

-- same for an error in an after-create script
create extension unaccent;
select current_user;
create extension pageinspect;
drop extension pageinspect;
\echo

-- a script that handles its own error keeps working
create extension tablefunc;
select current_user;
drop extension tablefunc;
create extension pageinspect;
drop extension pageinspect;
\echo

-- an error inside create extension itself, while elevated
create extension pageinspect schema no_such_schema;
select current_user;
create extension pageinspect;
drop extension pageinspect;
\echo

-- a statement timeout while a custom script is running
set statement_timeout = '200ms';
create extension tcn;
reset statement_timeout;
select current_user;
create extension pageinspect;
drop extension pageinspect;
\echo

-- a custom script that itself creates a privileged extension (nested elevation)
drop extension if exists citext;
create extension autoinc;
select current_user;
drop extension citext;
drop extension autoinc;

reset role;
