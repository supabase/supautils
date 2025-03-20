{ fetchFromGitHub, lib } :
let
  dep = fetchFromGitHub {
    owner  = "steve-chavez";
    repo   = "xpg";
    rev    = "v1.2";
    sha256 = "sha256-7sP+exW5CSh8c9NW4f8yr4bLcN5hJDxU5eWa8PjoNZA=";
  };
  xpg = (import dep).xpg;
in
xpg
