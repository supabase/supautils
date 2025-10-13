{ fetchFromGitHub, lib } :
let
  dep = fetchFromGitHub {
    owner  = "steve-chavez";
    repo   = "xpg";
    rev    = "v1.6.2";
    sha256 = "sha256-97RHjB0xwWsSQQoI+KWw/WZ3h0OyKiYD1jLIq2kqx7k=";
  };
  xpg = import dep;
in
xpg
