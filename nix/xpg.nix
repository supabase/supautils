{ fetchFromGitHub, lib } :
let
  dep = fetchFromGitHub {
    owner  = "steve-chavez";
    repo   = "xpg";
    rev    = "v1.10.1";
    sha256 = "sha256-KyRczrGL0tLYTs01YiA+w001ZZBbfoA1yzCfv77iIXU=";
  };
  xpg = import dep;
in
xpg
