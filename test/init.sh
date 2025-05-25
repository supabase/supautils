#!/usr/bin/env sh

# print notice when creating an extension
mkdir -p "$TMPDIR/extension-custom-scripts"
echo "do \$\$
      begin
        if not (@extname@ = ANY(ARRAY['dict_xsyn', 'insert_username'])) then
          return;
        end if;
        if exists (select from pg_available_extensions where name = @extname@) then
          raise notice 'extname: %, extschema: %, extversion: %, extcascade: %', @extname@, @extschema@, @extversion@, @extcascade@;
        end if;
      end \$\$;" > "$TMPDIR/extension-custom-scripts/before-create.sql"

mkdir -p "$TMPDIR/extension-custom-scripts/autoinc"
echo 'create extension citext;' > "$TMPDIR/extension-custom-scripts/autoinc/after-create.sql"

# assert both before-create and after-create scripts are run
mkdir -p "$TMPDIR/extension-custom-scripts/fuzzystrmatch"
echo 'create table t1();' > "$TMPDIR/extension-custom-scripts/fuzzystrmatch/before-create.sql"
echo 'drop table t1; create table t2 as values (1);' > "$TMPDIR/extension-custom-scripts/fuzzystrmatch/after-create.sql"
