{ lib, stdenv, fetchFromGitHub, postgresql }:

stdenv.mkDerivation rec {
  pname = "pgmq";
  version = "1.4.4";
  buildInputs = [ postgresql ];
  src = fetchFromGitHub {
    owner  = "tembo-io";
    repo   = pname;
    rev    = "v${version}";
    hash = "sha256-z+8/BqIlHwlMnuIzMz6eylmYbSmhtsNt7TJf/CxbdVw=";
  };

  buildPhase = ''
    cd pgmq-extension
  '';

  installPhase = ''
    install -D sql/pgmq.sql "$out/pgmq--${version}.sql"
    install -D -t $out sql/*.sql
    install -D -t $out *.control
  '';

  meta = with lib; {
    description = "A lightweight message queue. Like AWS SQS and RSMQ but on Postgres.";
    homepage    = "https://github.com/tembo-io/pgmq";
    platforms   = postgresql.meta.platforms;
    license     = licenses.postgresql;
  };
}
