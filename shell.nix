with import (builtins.fetchTarball {
  name = "23.11";
  url = "https://github.com/NixOS/nixpkgs/archive/refs/tags/23.11.tar.gz";
  sha256 = "1ndiv385w1qyb3b18vw13991fzb9wg4cl21wglk89grsfsnra41k";
}) {};
let
  supportedPgVersions = [
    postgresql_12
    postgresql_13
    postgresql_14
    postgresql_15
  ];
  pgWithExt = { postgresql }: postgresql.withPackages (p: [
    p.pg_cron
    (callPackage ./nix/supautils.nix { inherit postgresql; extraMakeFlags = "TEST=1"; })
    (callPackage ./nix/pg_tle.nix { inherit postgresql; })
  ]);
  pgScriptAll = map (x: callPackage ./nix/pgScript.nix { postgresql = pgWithExt { postgresql = x;}; }) supportedPgVersions;
in
mkShell {
  buildInputs = [ pgScriptAll ];
}
