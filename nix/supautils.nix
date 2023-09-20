{ stdenv, postgresql }:

stdenv.mkDerivation {
  name = "supautils";
  buildInputs = [ postgresql ];
  src = ../.;
  installPhase = ''
    mkdir -p $out/lib
    install -D supautils.so -t $out/lib
  '';
}
