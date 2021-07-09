create or replace function @extschema@.sql_to_ast(text) returns text
as 'supautils'
language C strict;
