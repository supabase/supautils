#!/usr/bin/env sh

# print notice when creating an extension
mkdir -p "/tmp/extension-custom-scripts"
echo "do \$\$
      declare
        _sp text;
      begin
        if (@extname@ = 'btree_gin') then
          raise notice 'global before-create.sql firing';
        end if;
        -- global script that pins search_path transaction-locally and
        -- restores it (the supabase_vault pattern)
        if @extname@ = 'pg_trgm' then
          _sp := current_setting('search_path');
          perform set_config('search_path', 'pg_catalog', true);
          perform set_config('search_path', _sp, true);
        end if;
        -- global script that replaces search_path at session level and
        -- never restores it
        if @extname@ = 'isn' then
          perform set_config('search_path', 'public', false);
        end if;
        -- global script that sets an unrelated GUC
        if @extname@ = 'pg_freespacemap' then
          perform set_config('work_mem', '64MB', false);
        end if;
        if not (@extname@ = ANY(ARRAY['dict_xsyn', 'insert_username'])) then
          return;
        end if;
        if exists (select from pg_available_extensions where name = @extname@) then
          raise notice 'extname: %, extschema: %, extversion: %, extcascade: %', @extname@, @extschema@, @extversion@, @extcascade@;
        end if;
      end \$\$;" > "/tmp/extension-custom-scripts/before-create.sql"

echo "do \$\$
      begin

      if (@extname@ = 'btree_gin') then
        raise notice 'global after-create.sql firing';
      end if;

      end \$\$;" > "/tmp/extension-custom-scripts/after-create.sql"

mkdir "/tmp/extension-custom-scripts/btree_gin"
echo "
do \$\$
      begin
        raise notice 'btree_gin before-create.sql firing';


end \$\$;
" > "/tmp/extension-custom-scripts/btree_gin/before-create.sql"

echo "
do \$\$
      begin
        raise notice 'btree_gin after-create.sql firing';
end \$\$;
" > "/tmp/extension-custom-scripts/btree_gin/after-create.sql"

mkdir -p "/tmp/extension-custom-scripts/autoinc"
echo 'create extension citext;' > "/tmp/extension-custom-scripts/autoinc/after-create.sql"

# assert both before-create and after-create scripts are run
mkdir -p "/tmp/extension-custom-scripts/fuzzystrmatch"
echo 'create table t1();' > "/tmp/extension-custom-scripts/fuzzystrmatch/before-create.sql"
echo 'drop table t1; create table t2 as values (1);' > "/tmp/extension-custom-scripts/fuzzystrmatch/after-create.sql"

# scripts that error while the backend is elevated, and one that handles its own error
mkdir -p "/tmp/extension-custom-scripts/ltree"
echo 'select 1/0;' > "/tmp/extension-custom-scripts/ltree/before-create.sql"
mkdir -p "/tmp/extension-custom-scripts/unaccent"
echo 'select 1/0;' > "/tmp/extension-custom-scripts/unaccent/after-create.sql"
mkdir -p "/tmp/extension-custom-scripts/tablefunc"
echo "do \$\$ begin perform 1/0; exception when division_by_zero then raise notice 'handled in script'; end \$\$;" > "/tmp/extension-custom-scripts/tablefunc/before-create.sql"
mkdir -p "/tmp/extension-custom-scripts/tcn"
echo 'select pg_sleep(5);' > "/tmp/extension-custom-scripts/tcn/before-create.sql"

# a script that pins search_path transaction-locally and restores it (the
# supabase_vault pattern)
mkdir -p "/tmp/extension-custom-scripts/dict_int"
echo "do \$\$
      declare
        _sp text := current_setting('search_path');
      begin
        perform set_config('search_path', 'pg_catalog', true);
        perform set_config('search_path', _sp, true);
      end \$\$;" > "/tmp/extension-custom-scripts/dict_int/before-create.sql"

# a script that replaces search_path at session level and never restores it
mkdir -p "/tmp/extension-custom-scripts/intarray"
echo "do \$\$
      begin
        perform set_config('search_path', 'public', false);
      end \$\$;" > "/tmp/extension-custom-scripts/intarray/before-create.sql"

# Self-recursion: bloom extension's before-create.sql creates itself
mkdir -p "/tmp/extension-custom-scripts/bloom"
echo "do \$\$
      begin
        create extension bloom;
      end \$\$;" > "/tmp/extension-custom-scripts/bloom/before-create.sql"

# Mutual-recursion: pgrowlocks extension's before-create.sql creates pgstattuple
# and vice-versa
mkdir -p "/tmp/extension-custom-scripts/pgrowlocks"
echo "do \$\$
      begin
        create extension pgstattuple;
      end \$\$;" > "/tmp/extension-custom-scripts/pgrowlocks/before-create.sql"

mkdir -p "/tmp/extension-custom-scripts/pgstattuple"
echo "do \$\$
      begin
        create extension pgrowlocks;
      end \$\$;" > "/tmp/extension-custom-scripts/pgstattuple/after-create.sql"

# Mutual-recusion with three extensions: pg_buffercache -> pg_visibility -> pg_prewarm -> pg_buffercache
mkdir -p "/tmp/extension-custom-scripts/pg_buffercache"
echo "do \$\$
      begin
        create extension pg_visibility;
      end \$\$;" > "/tmp/extension-custom-scripts/pg_buffercache/before-create.sql"

mkdir -p "/tmp/extension-custom-scripts/pg_visibility"
echo "do \$\$
      begin
        create extension pg_prewarm;
      end \$\$;" > "/tmp/extension-custom-scripts/pg_visibility/after-create.sql"

mkdir -p "/tmp/extension-custom-scripts/pg_prewarm"
echo "do \$\$
      begin
        create extension pg_buffercache;
      end \$\$;" > "/tmp/extension-custom-scripts/pg_prewarm/before-create.sql"

# Non-cyclic dependency: lo's before-create.sql creates tsm_system_rows.
# This should run both extensions' custom scripts.
mkdir -p "/tmp/extension-custom-scripts/lo"
echo "do \$\$
      begin
        raise notice 'before creating tsm_system_rows in lo''s before-create.sql script';
        create extension if not exists tsm_system_rows;
        raise notice 'after creating tsm_system_rows in lo''s before-create.sql script';
      end \$\$;" > "/tmp/extension-custom-scripts/lo/before-create.sql"

mkdir -p "/tmp/extension-custom-scripts/tsm_system_rows"
echo "do \$\$
      begin
        raise notice 'tsm_system_rows''s before-create.sql running';
      end \$\$;" > "/tmp/extension-custom-scripts/tsm_system_rows/before-create.sql"
echo "do \$\$
      begin
        raise notice 'tsm_system_rows''s after-create.sql running';
      end \$\$;" > "/tmp/extension-custom-scripts/tsm_system_rows/after-create.sql"
