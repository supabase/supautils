with import (builtins.fetchTarball {
  name = "2020-12-22";
  url = "https://github.com/NixOS/nixpkgs/archive/2a058487cb7a50e7650f1657ee0151a19c59ec3b.tar.gz";
  sha256 = "1h8c0mk6jlxdmjqch6ckj30pax3hqh6kwjlvp2021x3z4pdzrn9p";
}) {};
let
  check_role_membership = { postgresql }:
    stdenv.mkDerivation {
      name = "check_role_membership";
      buildInputs = [ postgresql ];
      src = ./check_role_membership;
      installPhase = ''
        mkdir -p $out/bin
        install -D check_role_membership.so -t $out/lib
      '';
    };
  pgWithExt = { postgresql } :
    let pg = postgresql.withPackages (p: [ (check_role_membership {inherit postgresql;}) ]);
    in ''
      tmpdir="$(mktemp -d)"

      export PGDATA="$tmpdir"
      export PGHOST="$tmpdir"
      export PGUSER=postgres
      export PGDATABASE=postgres

      trap '${pg}/bin/pg_ctl stop -m i && rm -rf "$tmpdir"' sigint sigterm exit

      PGTZ=UTC ${pg}/bin/initdb --no-locale --encoding=UTF8 --nosync -U "$PGUSER"
      ${pg}/bin/pg_ctl start -o "-F -c shared_preload_libraries=\"check_role_membership\" -c listen_addresses=\"\" -k $PGDATA"

      ${pg}/bin/psql
    '';
  supautils-pg-12 = pkgs.writeShellScriptBin "supautils-pg-12" (pgWithExt { postgresql = postgresql_12; });
  supautils-pg-13 = pkgs.writeShellScriptBin "supautils-pg-13" (pgWithExt { postgresql = postgresql_13; });
in
pkgs.mkShell {
  buildInputs = [ supautils-pg-12 supautils-pg-13 ];
}
