{ fetchFromGitHub, lib } :
let
  dep = fetchFromGitHub {
    owner  = "steve-chavez";
    repo   = "xpg";
    rev    = "v1.9.1";
    sha256 = "sha256-gL+vT2UxijObhTR7ziPsqHRYQ5EjIIZQzn8E6NQ/KGk=";
  };
  xpg = import dep;
in
xpg
