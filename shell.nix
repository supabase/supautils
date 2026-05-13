let
  flakeLock = builtins.fromJSON (builtins.readFile ./flake.lock);
  nixpkgsLock = flakeLock.nodes.nixpkgs.locked;
  xpgLock = flakeLock.nodes.xpg.locked;
in
{ pkgs ?
  import (builtins.fetchTarball {
    name = nixpkgsLock.rev;
    url = "https://github.com/${nixpkgsLock.owner}/${nixpkgsLock.repo}/archive/${nixpkgsLock.rev}.tar.gz";
    sha256 = nixpkgsLock.narHash;
  }) { }
, xpgPkgs ?
  import (pkgs.fetchFromGitHub {
    inherit (xpgLock) owner repo rev;
    sha256 = xpgLock.narHash;
  })
}:
let
  pgsqlcheck15 = pkgs.callPackage ./nix/plpgsql-check.nix {
    postgresql = xpgPkgs.postgresql_15;
  };
  pgmq15 = pkgs.callPackage ./nix/pgmq.nix {
    postgresql = xpgPkgs.postgresql_15;
  };
  style =
    pkgs.writeShellScriptBin "supautils-style" ''
      set -euo pipefail

      ${pkgs.clang-tools}/bin/clang-format -i src/*
    '';
  styleCheck =
    pkgs.writeShellScriptBin "supautils-style-check" ''
      set -euo pipefail

      ${pkgs.clang-tools}/bin/clang-format -i src/*
      ${pkgs.git}/bin/git diff-index --exit-code HEAD -- '*.c'
    '';
  loadtestUtility =
    pkgs.writeShellScriptBin "supautils-loadtest-utility" ''
      set -euo pipefail

      file=./bench/utility.sql

      common_opts="-n -T 60 -f $file"

      cat <<EOF
      Results without supautils:

      \`\`\`
      $(${xpgPkgs.xpg}/bin/xpg --options "-c wal_level=logical" pgbench $common_opts)
      \`\`\`

      Results with supautils and superuser:

      \`\`\`
      $(${xpgPkgs.xpg}/bin/xpg --options "-c wal_level=logical -c session_preload_libraries=supautils" pgbench $common_opts)
      \`\`\`

      Results with supautils and privileged_role:

      \`\`\`
      $(${xpgPkgs.xpg}/bin/xpg --options "-c wal_level=logical -c session_preload_libraries=supautils -c supautils.privileged_role='privileged_role' -c supautils.privileged_extensions=pg_trgm,postgres_fdw" pgbench -U privileged_role $common_opts)
      \`\`\`

      EOF
    '';
  loadtestSelect =
    pkgs.writeShellScriptBin "supautils-loadtest-select" ''
      set -euo pipefail

      init_opts="-i -s 10"
      common_opts="-S -T 120"

      cat <<EOF
      Results without supautils:

      \`\`\`
      $(${xpgPkgs.xpg}/bin/xpg --init-options "$init_opts" pgbench $common_opts)
      \`\`\`

      Results with supautils and no hint_role:

      \`\`\`
      $(${xpgPkgs.xpg}/bin/xpg --init-options "$init_opts" --options "-c session_preload_libraries=supautils" pgbench $common_opts)
      \`\`\`

      Results with supautils and hint_role:

      \`\`\`
      $(${xpgPkgs.xpg}/bin/xpg --init-options "$init_opts" --options "-c session_preload_libraries=supautils -c supautils.hint_roles='hint_role'" pgbench -U hint_role $common_opts)
      \`\`\`

      EOF
    '';
in
pkgs.mkShell {
  buildInputs = [
    (xpgPkgs.xpg.withExtensions {
      extensions = {
        "15" = [ pgsqlcheck15 pgmq15 ];
      };
    })
    style
    styleCheck
    loadtestUtility
    loadtestSelect
  ];
}
