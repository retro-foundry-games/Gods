# nixpkgs' SDL3 source compiled once for Emscripten (with threads: the game's program runs on a thread, so the page links shared memory), so game rebuilds reuse it.
{ pkgs }:
pkgs.stdenv.mkDerivation {
  pname = "sdl3-emscripten";
  inherit (pkgs.sdl3) version src;
  nativeBuildInputs = [
    pkgs.cmake
    pkgs.ninja
    pkgs.python3
    pkgs.emscripten
  ];
  dontUseCmakeConfigure = true;
  buildPhase = ''
    export HOME=$TMPDIR EM_CACHE=$TMPDIR/em-cache
    emcmake cmake -S . -B build-sdl -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$out \
      -DSDL_SHARED=OFF \
      -DSDL_STATIC=ON \
      -DSDL_TEST_LIBRARY=OFF \
      -DSDL_TESTS=OFF \
      -DSDL_INSTALL=ON \
      -DCMAKE_C_FLAGS=-pthread
    cmake --build build-sdl
  '';
  installPhase = ''
    cmake --install build-sdl
  '';
}
