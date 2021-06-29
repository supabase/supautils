with import (builtins.fetchTarball {
  name = "2020-12-22";
  url = "https://github.com/NixOS/nixpkgs/archive/2a058487cb7a50e7650f1657ee0151a19c59ec3b.tar.gz";
  sha256 = "1h8c0mk6jlxdmjqch6ckj30pax3hqh6kwjlvp2021x3z4pdzrn9p";
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
        install -D -t $out/share/postgresql/extension src/pg_supa--*.sql
        install -D -t $out/share/postgresql/extension pg_supa.control
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

      options="-F -c listen_addresses=\"\" -k $PGDATA"

      reserved_roles="supabase_storage_admin, anon, reserved_but_not_yet_created"
      reserved_memberships="pg_read_server_files, pg_write_server_files, pg_execute_server_program, role_with_reserved_membership"

      ext_options="-c shared_preload_libraries=\"supautils\" -c supautils.reserved_roles=\"$reserved_roles\" -c supautils.reserved_memberships=\"$reserved_memberships\""

      pg_ctl start -o "$options" -o "$ext_options"

      psql -v ON_ERROR_STOP=1 -f test/fixtures.sql

      "$@"
    '';
  supautils-with-pg-12 = writeShellScriptBin "supautils-with-pg-12" (pgWithExt { postgresql = postgresql_12; });
  supautils-with-pg-13 = writeShellScriptBin "supautils-with-pg-13" (pgWithExt { postgresql = postgresql_13; });
in
mkShell {
  buildInputs = [ supautils-with-pg-12 supautils-with-pg-13 ];
}
