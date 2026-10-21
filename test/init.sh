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
