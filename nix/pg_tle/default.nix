{
  lib,
  stdenv,
  fetchFromGitHub,
  flex,
  libkrb5,
  openssl,
  postgresql,
}:

stdenv.mkDerivation rec {
  pname = "pg_tle";
  version = "1.5.2";

  src = fetchFromGitHub {
    owner = "aws";
    repo = "pg_tle";
    rev = "v${version}";
    hash = "sha256-DB7aPSgW2/cjDWwXsFiEfJ5xhlHnhtII0quxtgwZg5c=";
  };

  # Use the patch which makes pg_tle look for extension files inside the directories
  # configured in the extension_control_path GUC. For details read the comments
  # inside the patch file.
  patches = [ ./patches/extension-control-path.patch ];

  nativeBuildInputs = [ flex ];
  buildInputs = [
    libkrb5
    openssl
    postgresql
  ];

  makeFlags = [ "FLEX=${flex}/bin/flex" ];

  installPhase = ''
    install -D -t $out *${postgresql.dlSuffix}
    install -D -t $out *.sql
    install -D -t $out *.control
    install -D -t $out/extension *.sql
    install -D -t $out/extension *.control
  '';

  meta = with lib; {
    description = "Trusted Language Extensions for PostgreSQL";
    homepage = "https://github.com/aws/pg_tle";
    platforms = postgresql.meta.platforms;
    license = licenses.asl20;
  };
}
