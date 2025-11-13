{ stdenv
, pkg-config
, llvmPackages
, autoconf
, doxygen
, graphviz-nox
}:

llvmPackages.stdenv.mkDerivation {
  pname = "luma";
  version = "0.0.0"; # TODO: versioning

  src = ./.;

  nativeBuildInputs = [
    pkg-config
    autoconf
    doxygen
    graphviz-nox
  ];

  outputs = [
    "out"
    "doc"
  ];

  preConfigure = ''
    autoconf
  '';

  postInstall = ''
    doxygen Doxyfile
    mv docs/doxygen $doc 
  '';
  
  buildInputs = [
    llvmPackages.libllvm
  ];

  doCheck = true;
}
