#!/usr/bin/env sh

# print notice when creating an extension
mkdir -p "$TMPDIR/extension-custom-scripts"
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

        -- pg_surgery is only created when this global script is run
        -- for tsm_system_time. This is not a cyclic dependency. It would have
        -- been a cyclic dependency only if the 'create extension ...' command
        -- unconditionally outside the if's block.
        if @extname@ = 'tsm_system_time' then
          create extension if not exists pg_surgery;
        end if;

        if not (@extname@ = ANY(ARRAY['dict_xsyn', 'insert_username'])) then
          return;
        end if;
        if exists (select from pg_available_extensions where name = @extname@) then
          raise notice 'extname: %, extschema: %, extversion: %, extcascade: %', @extname@, @extschema@, @extversion@, @extcascade@;
        end if;
      end \$\$;" > "$TMPDIR/extension-custom-scripts/before-create.sql"

echo "do \$\$
      begin

      if (@extname@ = 'btree_gin') then
        raise notice 'global after-create.sql firing';
      end if;

      end \$\$;" > "$TMPDIR/extension-custom-scripts/after-create.sql"

mkdir "$TMPDIR/extension-custom-scripts/btree_gin"
echo "
do \$\$
      begin
        raise notice 'btree_gin before-create.sql firing';


end \$\$;
" > "$TMPDIR/extension-custom-scripts/btree_gin/before-create.sql"

echo "
do \$\$
      begin
        raise notice 'btree_gin after-create.sql firing';
end \$\$;
" > "$TMPDIR/extension-custom-scripts/btree_gin/after-create.sql"

mkdir -p "$TMPDIR/extension-custom-scripts/autoinc"
echo 'create extension citext;' > "$TMPDIR/extension-custom-scripts/autoinc/after-create.sql"

# assert both before-create and after-create scripts are run
mkdir -p "$TMPDIR/extension-custom-scripts/fuzzystrmatch"
echo 'create table t1();' > "$TMPDIR/extension-custom-scripts/fuzzystrmatch/before-create.sql"
echo 'drop table t1; create table t2 as values (1);' > "$TMPDIR/extension-custom-scripts/fuzzystrmatch/after-create.sql"

# scripts that error while the backend is elevated, and one that handles its own error
mkdir -p "$TMPDIR/extension-custom-scripts/ltree"
echo 'select 1/0;' > "$TMPDIR/extension-custom-scripts/ltree/before-create.sql"
mkdir -p "$TMPDIR/extension-custom-scripts/unaccent"
echo 'select 1/0;' > "$TMPDIR/extension-custom-scripts/unaccent/after-create.sql"
mkdir -p "$TMPDIR/extension-custom-scripts/tablefunc"
echo "do \$\$ begin perform 1/0; exception when division_by_zero then raise notice 'handled in script'; end \$\$;" > "$TMPDIR/extension-custom-scripts/tablefunc/before-create.sql"
mkdir -p "$TMPDIR/extension-custom-scripts/tcn"
echo 'select pg_sleep(5);' > "$TMPDIR/extension-custom-scripts/tcn/before-create.sql"

# a script that pins search_path transaction-locally and restores it (the
# supabase_vault pattern)
mkdir -p "$TMPDIR/extension-custom-scripts/dict_int"
echo "do \$\$
      declare
        _sp text := current_setting('search_path');
      begin
        perform set_config('search_path', 'pg_catalog', true);
        perform set_config('search_path', _sp, true);
      end \$\$;" > "$TMPDIR/extension-custom-scripts/dict_int/before-create.sql"

# a script that replaces search_path at session level and never restores it
mkdir -p "$TMPDIR/extension-custom-scripts/intarray"
echo "do \$\$
      begin
        perform set_config('search_path', 'public', false);
      end \$\$;" > "$TMPDIR/extension-custom-scripts/intarray/before-create.sql"

# Self-recursion: bloom extension's before-create.sql creates itself
mkdir -p "$TMPDIR/extension-custom-scripts/bloom"
echo "do \$\$
      begin
        create extension bloom;
      end \$\$;" > "$TMPDIR/extension-custom-scripts/bloom/before-create.sql"

# Mutual-recursion: pgrowlocks extension's before-create.sql creates pgstattuple
# and vice-versa
mkdir -p "$TMPDIR/extension-custom-scripts/pgrowlocks"
echo "do \$\$
      begin
        create extension pgstattuple;
      end \$\$;" > "$TMPDIR/extension-custom-scripts/pgrowlocks/before-create.sql"

mkdir -p "$TMPDIR/extension-custom-scripts/pgstattuple"
echo "do \$\$
      begin
        create extension pgrowlocks;
      end \$\$;" > "$TMPDIR/extension-custom-scripts/pgstattuple/after-create.sql"

# Mutual-recusion with three extensions: pg_buffercache -> pg_visibility -> pg_prewarm -> pg_buffercache
mkdir -p "$TMPDIR/extension-custom-scripts/pg_buffercache"
echo "do \$\$
      begin
        create extension pg_visibility;
      end \$\$;" > "$TMPDIR/extension-custom-scripts/pg_buffercache/before-create.sql"

mkdir -p "$TMPDIR/extension-custom-scripts/pg_visibility"
echo "do \$\$
      begin
        create extension pg_prewarm;
      end \$\$;" > "$TMPDIR/extension-custom-scripts/pg_visibility/after-create.sql"

mkdir -p "$TMPDIR/extension-custom-scripts/pg_prewarm"
echo "do \$\$
      begin
        create extension pg_buffercache;
      end \$\$;" > "$TMPDIR/extension-custom-scripts/pg_prewarm/before-create.sql"

# Non-cyclic dependency: amcheck's before-create.sql creates tsm_system_rows.
# This should run both extensions' custom scripts.
mkdir -p "$TMPDIR/extension-custom-scripts/amcheck"
echo "do \$\$
      begin
        raise notice 'before creating tsm_system_rows in amcheck''s before-create.sql script';
        create extension if not exists tsm_system_rows;
        raise notice 'after creating tsm_system_rows in amcheck''s before-create.sql script';
      end \$\$;" > "$TMPDIR/extension-custom-scripts/amcheck/before-create.sql"

mkdir -p "$TMPDIR/extension-custom-scripts/tsm_system_rows"
echo "do \$\$
      begin
        raise notice 'tsm_system_rows''s before-create.sql running';
      end \$\$;" > "$TMPDIR/extension-custom-scripts/tsm_system_rows/before-create.sql"
echo "do \$\$
      begin
        raise notice 'tsm_system_rows''s after-create.sql running';
      end \$\$;" > "$TMPDIR/extension-custom-scripts/tsm_system_rows/after-create.sql"
