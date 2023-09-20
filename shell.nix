with import (builtins.fetchTarball {
  name = "2023-09-16";
  url = "https://github.com/NixOS/nixpkgs/archive/ae5b96f3ab6aabb60809ab78c2c99f8dd51ee678.tar.gz";
  sha256 = "11fpdcj5xrmmngq0z8gsc3axambqzvyqkfk23jn3qkx9a5x56xxk";
}) {};
let
  supportedPgVersions = [
    postgresql_12
    postgresql_13
    postgresql_14
    postgresql_15
    postgresql_16
  ];
  pgWithExt = { postgresql }: postgresql.withPackages (p: [
    (callPackage ./nix/supautils.nix { inherit postgresql; extraMakeFlags = "TEST=1"; })
    (callPackage ./nix/pg_tle.nix { inherit postgresql; })
  ]);
  pgScriptAll = map (x: callPackage ./nix/pgScript.nix { postgresql = pgWithExt { postgresql = x;}; }) supportedPgVersions;
in
mkShell {
  buildInputs = [ pgScriptAll ];
}
