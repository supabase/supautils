{ fetchFromGitHub, lib } :
let
  dep = fetchFromGitHub {
    owner  = "steve-chavez";
    repo   = "xpg";
    rev    = "v1.7.0";
    sha256 = "sha256-TWFK1u1UX7cm43BJGRbDtYKzbHF37pl+pMelxujqdjE=";
  };
  xpg = import dep;
in
xpg
