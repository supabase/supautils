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
      loadtestUtility =
        writeShellScriptBin "supautils-loadtest-utility" ''
          set -euo pipefail

          file=./bench/utility.sql

          common_opts="-n -T 60 -f $file"

          cat <<EOF
          Results without supautils:

          \`\`\`
          $(${xpg.xpg}/bin/xpg --options "-c wal_level=logical" pgbench $common_opts)
          \`\`\`

          Results with supautils and superuser:

          \`\`\`
          $(${xpg.xpg}/bin/xpg --options "-c wal_level=logical -c session_preload_libraries=supautils" pgbench $common_opts)
          \`\`\`

          Results with supautils and privileged_role:

          \`\`\`
          $(${xpg.xpg}/bin/xpg --options "-c wal_level=logical -c session_preload_libraries=supautils -c supautils.privileged_role='privileged_role' -c supautils.privileged_extensions=pg_trgm,postgres_fdw" pgbench -U privileged_role $common_opts)
          \`\`\`

          EOF
        '';
      loadtestSelect =
        writeShellScriptBin "supautils-loadtest-select" ''
          set -euo pipefail

          init_opts="-i -s 10"
          common_opts="-S -T 120"

          cat <<EOF
          Results without supautils:

          \`\`\`
          $(${xpg.xpg}/bin/xpg --init-options "$init_opts" pgbench $common_opts)
          \`\`\`

          Results with supautils and superuser:

          \`\`\`
          $(${xpg.xpg}/bin/xpg --init-options "$init_opts" --options "-c session_preload_libraries=supautils" pgbench $common_opts)
          \`\`\`

          Results with supautils and privileged_role:

          \`\`\`
          $(${xpg.xpg}/bin/xpg --init-options "$init_opts" --options "-c session_preload_libraries=supautils -c supautils.privileged_role='privileged_role'" pgbench -U privileged_role $common_opts)
          \`\`\`

          EOF
        '';
    in
    [
      (xpg.xpgWithExtensions {
        exts15 = [ pgsqlcheck15 pgmq15 ];
      })
      style
      styleCheck
      loadtestUtility
      loadtestSelect
    ];
}
