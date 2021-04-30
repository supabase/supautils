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
      '';
    };
  pgWithExt = { postgresql } :
    let pg = postgresql.withPackages (p: [ (supautils {inherit postgresql;}) ]);
    in ''
      tmpdir="$(mktemp -d)"

      export PGDATA="$tmpdir"
      export PGHOST="$tmpdir"
      export PGUSER=postgres
      export PGDATABASE=postgres

      trap '${pg}/bin/pg_ctl stop -m i && rm -rf "$tmpdir"' sigint sigterm exit

      PGTZ=UTC ${pg}/bin/initdb --no-locale --encoding=UTF8 --nosync -U "$PGUSER"
      ${pg}/bin/pg_ctl start -o "-F -c shared_preload_libraries=\"supautils\" -c listen_addresses=\"\" -k $PGDATA"

      ${pg}/bin/createuser -d --no-inherit --no-createdb --createrole nosuper

      ${pg}/bin/psql -U nosuper
    '';
  supautils-pg-12 = pkgs.writeShellScriptBin "supautils-pg-12" (pgWithExt { postgresql = postgresql_12; });
  supautils-pg-13 = pkgs.writeShellScriptBin "supautils-pg-13" (pgWithExt { postgresql = postgresql_13; });
in
pkgs.mkShell {
  buildInputs = [ supautils-pg-12 supautils-pg-13 ];
}
