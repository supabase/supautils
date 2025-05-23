set role extensions_role;
\echo

-- per-extension custom scripts are run
create extension fuzzystrmatch;
drop extension fuzzystrmatch;
select * from t2;

reset role;
drop table t2;
set role extensions_role;
\echo

-- global extension custom scripts are run
create extension dict_xsyn;
create extension insert_username version "1.0" schema public cascade;
\echo

-- custom scripts are run even for superusers
reset role;
create extension fuzzystrmatch;
drop extension fuzzystrmatch;
select * from t2;

drop table t2;
set role extensions_role;
\echo
