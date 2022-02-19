with import (builtins.fetchTarball {
  name = "2021-09-29";
  url = "https://github.com/NixOS/nixpkgs/archive/76b1e16c6659ccef7187ca69b287525fea133244.tar.gz";
  sha256 = "1vsahpcx80k2bgslspb0sa6j4bmhdx77sw6la455drqcrqhdqj6a";
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

      reserved_stuff_options="-c supautils.reserved_roles=\"$reserved_roles\" -c supautils.reserved_memberships=\"$reserved_memberships\""
      placeholder_stuff_options="-c supautils.placeholders=\"response.headers, another.placeholder\" -c supautils.placeholders_disallowed_values=\"content-type, x-special-header, special-value\""

      pg_ctl start -o "$options" -o "$reserved_stuff_options" -o "$placeholder_stuff_options"

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
