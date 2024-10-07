with import (builtins.fetchTarball {
  name = "24.05";
  url = "https://github.com/NixOS/nixpkgs/archive/refs/tags/24.05.tar.gz";
  sha256 = "sha256:1lr1h35prqkd1mkmzriwlpvxcb34kmhc9dnr48gkm8hh089hifmx";
}) {};
let
  ourPg = callPackage ./nix/postgresql/default.nix {
    inherit lib;
    inherit stdenv;
    inherit fetchurl;
    inherit makeWrapper;
    inherit callPackage;
  };
  supportedPgVersions = [
    postgresql_13
    postgresql_14
    postgresql_15
    postgresql_16
    ourPg.postgresql_17
  ];

  pgWithExt = { postgresql }: postgresql.withPackages (p: [
    (callPackage ./nix/supautils.nix { inherit postgresql; extraMakeFlags = "TEST=1"; })
    (callPackage ./nix/pg_tle.nix { inherit postgresql; })
  ]);
  pgScriptAll = map (x: callPackage ./nix/pgScript.nix { postgresql = pgWithExt { postgresql = x;}; }) supportedPgVersions;
in
mkShell {
  buildInputs = [
    pgScriptAll
  ];
}
