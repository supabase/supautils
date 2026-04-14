-- Test: non-superuser cannot specify VERSION in CREATE EXTENSION
set role extensions_role;

-- should fail: VERSION clause not allowed for non-superuser
create extension hstore version '1.4';
\echo

-- should succeed: no VERSION clause
create extension hstore;
\echo

-- should succeed: ALTER EXTENSION UPDATE without TO clause is allowed
alter extension hstore update;
\echo

-- should fail: UPDATE TO clause not allowed for non-superuser
alter extension hstore update to '1.4';
\echo

-- should fail: IF NOT EXISTS with VERSION still blocked
create extension if not exists hstore version '1.4';
\echo

-- cleanup
drop extension hstore;
\echo

-- Test: configured supautils.superuser CAN specify VERSION
-- superuser_for_test is nosuperuser but configured as supautils.superuser,
-- so this exercises the name comparison bypass path.
reset role;
set role superuser_for_test;

-- should succeed: CREATE EXTENSION with VERSION
create extension hstore version '1.4';

-- should succeed: ALTER EXTENSION UPDATE TO
alter extension hstore update to '1.8';

drop extension hstore;
\echo

reset role;
