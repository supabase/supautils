{ stdenv, postgresql, extraMakeFlags ? "" }:

stdenv.mkDerivation {
  name = "supautils";
  buildInputs = [ postgresql ];
  src = ../.;
  makeFlags = [ extraMakeFlags ];
  installPhase = ''
    mkdir -p $out/lib
    install -D *.{dylib,so} -t $out/lib
  '';
}
