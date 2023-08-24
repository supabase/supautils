with import (builtins.fetchTarball {
  name = "2022-10-25";
  url = "https://github.com/NixOS/nixpkgs/archive/a11f8032aa9de58be11190b71320f98f9a3c395b.tar.gz";
  sha256 = "101y90kqqfqc5vkigw5rbcqw01cg9nndknz4q4gb28zi4918r1hz";
}) {};
let
  supautils = { postgresql }:
    stdenv.mkDerivation {
      name = "supautils";
      buildInputs = [ postgresql ];
      src = ./.;
      installPhase = ''
        mkdir -p $out/bin
        install -D supautils.so -t $out/lib

        install -D -t $out/share/postgresql/extension supautils.control
      '';
    };
  pgWithExt = { postgresql } :
  let
    pg = postgresql.withPackages (p: [
      (supautils { inherit postgresql; })
      (callPackage ./nix/pg_tle.nix { inherit postgresql; })
    ]);
    ver = builtins.head (builtins.splitVersion postgresql.version);
    script = ''
      export PATH=${pg}/bin:"$PATH"

      tmpdir="$(mktemp -d)"

      export PGDATA="$tmpdir"
      export PGHOST="$tmpdir"
      export PGUSER=postgres
      export PGDATABASE=postgres

      trap 'pg_ctl stop -m i && rm -rf "$tmpdir"' sigint sigterm exit

      PGTZ=UTC initdb --no-locale --encoding=UTF8 --nosync -U "$PGUSER"

      options="-F -c listen_addresses=\"\" -k $PGDATA -c shared_preload_libraries=\"pg_tle, supautils\" -c wal_level=logical"

      reserved_roles="supabase_storage_admin, anon, reserved_but_not_yet_created, authenticator*"
      reserved_memberships="pg_read_server_files, pg_write_server_files, pg_execute_server_program, role_with_reserved_membership"
      privileged_extensions="hstore, postgres_fdw, pg_tle"
      privileged_extensions_custom_scripts_path="$tmpdir/privileged_extensions_custom_scripts"
      privileged_role="privileged_role"
      privileged_role_allowed_configs="session_replication_role, pgrst.*, other.nested.*"

      reserved_stuff_options="-c supautils.reserved_roles=\"$reserved_roles\" -c supautils.reserved_memberships=\"$reserved_memberships\" -c supautils.privileged_extensions=\"$privileged_extensions\" -c supautils.privileged_extensions_custom_scripts_path=\"$privileged_extensions_custom_scripts_path\" -c supautils.privileged_role=\"$privileged_role\" -c supautils.privileged_role_allowed_configs=\"$privileged_role_allowed_configs\""
      placeholder_stuff_options='-c supautils.placeholders="response.headers, another.placeholder" -c supautils.placeholders_disallowed_values="\"content-type\",\"x-special-header\",special-value"'

      pg_ctl start -o "$options" -o "$reserved_stuff_options" -o "$placeholder_stuff_options"

      mkdir -p "$tmpdir/privileged_extensions_custom_scripts/hstore"
      echo "do \$\$
            begin
              if not exists (select from pg_extension where extname = 'pg_tle') then
                return;
              end if;
              if exists (select from pgtle.available_extensions() where name = @extname@) then
                raise notice 'extname: %, extschema: %, extversion: %, extcascade: %', @extname@, @extschema@, @extversion@, @extcascade@;
              end if;
            end \$\$;" > "$tmpdir/privileged_extensions_custom_scripts/before-create.sql"
      echo 'create table t1();' > "$tmpdir/privileged_extensions_custom_scripts/hstore/before-create.sql"
      echo 'drop table t1; create table t2 as values (1);' > "$tmpdir/privileged_extensions_custom_scripts/hstore/after-create.sql"

      createdb contrib_regression

      psql -v ON_ERROR_STOP=1 -f test/fixtures.sql -d contrib_regression

      "$@"
    '';
  in
    writeShellScriptBin "supautils-with-pg-${ver}" script;
  supportedPgVersions = [  postgresql_12 postgresql_13 postgresql_14 postgresql_15 ];
  extAll = map (x: pgWithExt { postgresql = x;}) supportedPgVersions;
in
mkShell {
  buildInputs = [ extAll ];
}
