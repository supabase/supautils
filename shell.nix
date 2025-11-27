with import (builtins.fetchTarball {
  name = "2025-11-13";
  url = "https://github.com/NixOS/nixpkgs/archive/91c9a64ce2a84e648d0cf9671274bb9c2fb9ba60.tar.gz";
  sha256 = "sha256:19myp93spfsf5x62k6ncan7020bmbn80kj4ywcykqhb9c3q8fdr1";
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
      style =
        writeShellScriptBin "supautils-style" ''
          set -euo pipefail

          ${clang-tools}/bin/clang-format -i src/*
        '';
      styleCheck =
        writeShellScriptBin "supautils-style-check" ''
          set -euo pipefail

          ${clang-tools}/bin/clang-format -i src/*
          ${git}/bin/git diff-index --exit-code HEAD -- '*.c'
        '';
    in
    [
      (xpg.xpgWithExtensions {
        exts15 = [ pgsqlcheck15 pgmq15 ];
      })
      style
      styleCheck
    ];
}
