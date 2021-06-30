-- build extension
create schema supa;
create extension pg_supa with schema supa;

select supa.get_one();
