with import (builtins.fetchTarball {
  name = "2020-12-22";
  url = "https://github.com/NixOS/nixpkgs/archive/2a058487cb7a50e7650f1657ee0151a19c59ec3b.tar.gz";
  sha256 = "1h8c0mk6jlxdmjqch6ckj30pax3hqh6kwjlvp2021x3z4pdzrn9p";
}) {};
let
  check_role_membership = stdenv.mkDerivation {
    name = "check_role_membership";
    buildInputs = [ postgresql_13 ];
    src = ./check_role_membership;
    installPhase = ''
      mkdir -p $out/bin
      install -D check_role_membership.so -t $out/lib
    '';
  };
in
stdenv.mkDerivation {
  name = "supautils";
  buildInputs = [ (postgresql_13.withPackages (p: [ check_role_membership ])) ];
  shellHook = ''
    tmpdir="$(mktemp -d)"
    trap 'rm -rf "$tmpdir"' sigint sigterm exit

    export PGDATA="$tmpdir"
    export PGHOST="$tmpdir"
    export PGUSER=authenticator
    export PGDATABASE=postgres

    PGTZ=UTC initdb --no-locale --encoding=UTF8 --nosync -U "$PGUSER"
    pg_ctl start -o "-F -c shared_preload_libraries=\"check_role_membership\" -c listen_addresses=\"\" -k $PGDATA"
  '';
}
