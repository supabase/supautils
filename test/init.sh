# print notice when creating an extension
mkdir -p "$TMPDIR/privileged_extensions_custom_scripts"
echo "do \$\$
      begin
        if not (@extname@ = ANY(ARRAY['dict_xsyn', 'insert_username'])) then
          return;
        end if;
        if exists (select from pg_available_extensions where name = @extname@) then
          raise notice 'extname: %, extschema: %, extversion: %, extcascade: %', @extname@, @extschema@, @extversion@, @extcascade@;
        end if;
      end \$\$;" > "$TMPDIR/privileged_extensions_custom_scripts/before-create.sql"

mkdir -p "$TMPDIR/privileged_extensions_custom_scripts/autoinc"
echo 'create extension citext;' > "$TMPDIR/privileged_extensions_custom_scripts/autoinc/after-create.sql"

# assert both before-create and after-create scripts are run
mkdir -p "$TMPDIR/privileged_extensions_custom_scripts/hstore"
echo 'create table t1();' > "$TMPDIR/privileged_extensions_custom_scripts/hstore/before-create.sql"
echo 'drop table t1; create table t2 as values (1);' > "$TMPDIR/privileged_extensions_custom_scripts/hstore/after-create.sql"
