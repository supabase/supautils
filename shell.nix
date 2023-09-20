with import (builtins.fetchTarball {
  name = "2022-10-25";
  url = "https://github.com/NixOS/nixpkgs/archive/a11f8032aa9de58be11190b71320f98f9a3c395b.tar.gz";
  sha256 = "101y90kqqfqc5vkigw5rbcqw01cg9nndknz4q4gb28zi4918r1hz";
}) {};
let
  supportedPgVersions = [
    postgresql_12
    postgresql_13
    postgresql_14
    postgresql_15
  ];
  pgWithExt = { postgresql }: postgresql.withPackages (p: [
    (callPackage ./nix/supautils.nix { inherit postgresql; })
    (callPackage ./nix/pg_tle.nix { inherit postgresql; })
  ]);
  pgScriptAll = map (x: callPackage ./nix/pgScript.nix { postgresql = pgWithExt { postgresql = x;}; }) supportedPgVersions;
in
mkShell {
  buildInputs = [  pgScriptAll ];
}
