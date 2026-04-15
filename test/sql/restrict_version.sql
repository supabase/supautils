-- Test: non-superuser cannot specify VERSION in CREATE EXTENSION
set role extensions_role;

-- should fail: VERSION clause not allowed for non-superuser
create extension hstore version '1.4';
\echo

-- should succeed: no VERSION clause
create extension hstore;
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

-- Test: superuser CAN specify VERSION
reset role;

-- should succeed: CREATE EXTENSION with VERSION
create extension hstore version '1.4';

-- should succeed: ALTER EXTENSION UPDATE TO
alter extension hstore update to '1.7';

drop extension hstore;
\echo

reset role;
