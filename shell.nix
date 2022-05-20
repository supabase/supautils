with import (builtins.fetchTarball {
  name = "2021-09-29";
  url = "https://github.com/NixOS/nixpkgs/archive/76b1e16c6659ccef7187ca69b287525fea133244.tar.gz";
  sha256 = "1vsahpcx80k2bgslspb0sa6j4bmhdx77sw6la455drqcrqhdqj6a";
}) {
  overlays = [
    (self: super: {
      postgresql_14 = super.postgresql_14.overrideAttrs(oldAttrs: {
        configureFlags = oldAttrs.configureFlags ++ ["--enable-cassert"];
        patches = oldAttrs.patches ++ [
          ./0001-Assert-name-short_desc-to-prevent-SHOWALL-segfault.patch
        ];
      });
    })
  ];
};
let
  supautils = { postgresql }:
    stdenv.mkDerivation {
      name = "supautils";
      buildInputs = [ postgresql ];
      src = ./.;
      installPhase = ''
        mkdir -p $out/bin
        install -D supautils.so -t $out/lib

        # Build stdlib
        ./bin/build_stdlib.sh

        install -D -t $out/share/postgresql/extension sql/supautils--*.sql
        install -D -t $out/share/postgresql/extension supautils.control
      '';
    };
  pgWithExt = { postgresql } :
    let pg = postgresql.withPackages (p: [ (supautils {inherit postgresql;}) ]);
    in ''
      export PATH=${pg}/bin:"$PATH"

      tmpdir="$(mktemp -d)"

      export PGDATA="$tmpdir"
      export PGHOST="$tmpdir"
      export PGUSER=postgres
      export PGDATABASE=postgres

      trap 'pg_ctl stop -m i && rm -rf "$tmpdir"' sigint sigterm exit

      PGTZ=UTC initdb --no-locale --encoding=UTF8 --nosync -U "$PGUSER"

      options="-F -c listen_addresses=\"\" -k $PGDATA -c shared_preload_libraries=\"supautils\""

      reserved_roles="supabase_storage_admin, anon, reserved_but_not_yet_created"
      reserved_memberships="pg_read_server_files, pg_write_server_files, pg_execute_server_program, role_with_reserved_membership"
      privileged_extensions="hstore"
      privileged_extensions_custom_scripts_path="$tmpdir/privileged_extensions_custom_scripts"

      reserved_stuff_options="-c supautils.reserved_roles=\"$reserved_roles\" -c supautils.reserved_memberships=\"$reserved_memberships\" -c supautils.privileged_extensions=\"$privileged_extensions\" -c supautils.privileged_extensions_custom_scripts_path=\"$privileged_extensions_custom_scripts_path\""
      placeholder_stuff_options="-c supautils.placeholders=\"response.headers, another.placeholder\" -c supautils.placeholders_disallowed_values=\"content-type, x-special-header, special-value\""

      pg_ctl start -o "$options" -o "$reserved_stuff_options" -o "$placeholder_stuff_options"

      mkdir -p "$tmpdir/privileged_extensions_custom_scripts/hstore"
      echo 'create table t1();' > "$tmpdir/privileged_extensions_custom_scripts/hstore/before-create.sql"
      echo 'drop table t1; create table t2 as values (1);' > "$tmpdir/privileged_extensions_custom_scripts/hstore/after-create.sql"

      createdb contrib_regression

      psql -v ON_ERROR_STOP=1 -f test/fixtures.sql -d contrib_regression

      "$@"
    '';
  supautils-with-pg-12 = writeShellScriptBin "supautils-with-pg-12" (pgWithExt { postgresql = postgresql_12; });
  supautils-with-pg-13 = writeShellScriptBin "supautils-with-pg-13" (pgWithExt { postgresql = postgresql_13; });
  supautils-with-pg-14 = writeShellScriptBin "supautils-with-pg-14" (pgWithExt { postgresql = postgresql_14; });
in
mkShell {
  buildInputs = [ supautils-with-pg-12 supautils-with-pg-13 supautils-with-pg-14];
}
