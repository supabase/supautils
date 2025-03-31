{ lib, stdenv, fetchFromGitHub, postgresql }:

stdenv.mkDerivation rec {
  pname = "plpgsql-check";
  version = "2.7.11";

  src = fetchFromGitHub {
    owner = "okbob";
    repo = "plpgsql_check";
    rev = "v${version}";
    hash = "sha256-vR3MvfmUP2QEAtXFpq0NCCKck3wZPD+H3QleHtyVQJs=";
  };

  buildInputs = [ postgresql ];

  installPhase = ''
    install -D -t $out *${postgresql.dlSuffix}
    install -D -t $out *.sql
    install -D -t $out *.control
  '';

  meta = with lib; {
    description = "Linter tool for language PL/pgSQL";
    homepage = "https://github.com/okbob/plpgsql_check";
    changelog = "https://github.com/okbob/plpgsql_check/releases/tag/v${version}";
    platforms = postgresql.meta.platforms;
    license = licenses.mit;
    maintainers = [ maintainers.marsam ];
  };
}
