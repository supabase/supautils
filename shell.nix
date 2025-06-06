with import (builtins.fetchTarball {
  name = "24.05";
  url = "https://github.com/NixOS/nixpkgs/archive/refs/tags/24.05.tar.gz";
  sha256 = "sha256:1lr1h35prqkd1mkmzriwlpvxcb34kmhc9dnr48gkm8hh089hifmx";
}) {};
mkShell {
  buildInputs =
    let
      xpg = callPackage ./nix/xpg.nix {};
      pgsqlcheck15 = callPackage ./nix/plpgsql-check.nix {
        postgresql = xpg.postgresql_15;
      };
      pgmq15 = callPackage ./nix/pgmq.nix {
        postgresql = xpg.postgresql_15;
      };
    in
    [
      (xpg.xpgWithExtensions {
        exts15 = [ pgsqlcheck15 pgmq15 ];
      })
    ];
}
