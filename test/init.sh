#!/usr/bin/env sh

# print notice when creating an extension
mkdir -p "$TMPDIR/extension-custom-scripts"
echo "do \$\$
      begin
        PERFORM set_config('search_path', 'extensions, ' || current_setting('search_path'), false);

        if (@extname@ = 'btree_gin') then
          raise notice 'global before-create.sql firing';
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

      PERFORM set_config('search_path', regexp_replace(current_setting('search_path'), '^extensions, ', ''), false );
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

mkdir -p "$TMPDIR/extension-custom-scripts/pg_visibility"
echo "
do \$\$
      begin
      CREATE SCHEMA IF NOT EXISTS visibility;
      PERFORM set_config('search_path', 'visibility, ' || current_setting('search_path'), false);
      end 
\$\$;" > "$TMPDIR/extension-custom-scripts/pg_visibility/before-create.sql"

echo "
do \$\$
      begin
      PERFORM set_config('search_path', regexp_replace(current_setting('search_path'), '^visibility, ', ''), false);
      end 
\$\$;
" > "$TMPDIR/extension-custom-scripts/pg_visibility/after-create.sql"
