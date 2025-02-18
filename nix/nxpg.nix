{ stdenv, lib, makeWrapper, fetchurl, writeShellScriptBin, findutils, entr, callPackage, lcov } :
let
  prefix = "nxpg";
  ourPg = callPackage ./postgresql {
    inherit lib;
    inherit stdenv;
    inherit fetchurl;
    inherit makeWrapper;
    inherit callPackage;
  };
  supportedPgs = [
    ourPg.postgresql_17
    ourPg.postgresql_16
    ourPg.postgresql_15
    ourPg.postgresql_14
    ourPg.postgresql_13
  ];
  build =
    writeShellScriptBin "${prefix}-build" ''
      set -euo pipefail

      make clean
      make TEST=1
    '';
  buildCov =
    writeShellScriptBin "${prefix}-build-cov" ''
      set -euo pipefail

      make clean
      make TEST=1 COVERAGE=1
    '';
  test =
    writeShellScriptBin "${prefix}-test" ''
      set -euo pipefail

      make installcheck
    '';
  cov =
    writeShellScriptBin "${prefix}-coverage" ''
      set -euo pipefail

      info_file="coverage.info"
      out_dir="coverage_html"

      make installcheck
      ${lcov}/bin/lcov --capture --directory . --output-file "$info_file"

      # remove postgres headers on the nix store, otherwise they show on the output
      ${lcov}/bin/lcov --remove "$info_file" '/nix/*' --output-file "$info_file" || true

      ${lcov}/bin/lcov --list coverage.info
      ${lcov}/bin/genhtml "$info_file" --output-directory "$out_dir"

      echo "${prefix}-coverage: To see the results, visit file://$(pwd)/$out_dir/index.html on your browser"
    '';
  watch =
    writeShellScriptBin "${prefix}-watch" ''
      set -euo pipefail

      ${findutils}/bin/find . -type f \( -name '*.c' -o -name '*.h' \) | ${entr}/bin/entr -dr "$@"
    '';

  tmpDb =
    writeShellScriptBin "${prefix}-tmp" (builtins.readFile ./withTmpDb.sh.in);

  allPgPaths = map (pg:
      let
        ver = builtins.head (builtins.splitVersion pg.version);
        script = ''
          set -euo pipefail

          export PATH=${pg}/bin:"$PATH"

          "$@"
        '';
      in
      writeShellScriptBin "${prefix}-${ver}" script
    ) supportedPgs;
in
[
  build
  buildCov
  test
  cov
  watch
  tmpDb
  allPgPaths
]
