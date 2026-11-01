{
  description = "Gods native and browser builds";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    m68k-amigaos-gcc.url = "github:AmigaPorts/m68k-amigaos-gcc";
  };

  outputs = { self, nixpkgs, m68k-amigaos-gcc }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAll = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
      src = pkgs: pkgs.lib.cleanSourceWith {
        src = ./.;
        filter = path: type:
          let name = baseNameOf path; in
          !(builtins.elem name [ "build" "local" "captures" "saves-native" ".git" ".vscode" "result" ]);
      };
    in {
      packages = forAll (pkgs: {
        # Desktop build against nixpkgs' SDL3, then retargeted at the system
        # loader with no rpath so a host-installed libSDL3.so.0 takes over. The game's data structures
        # have the 68000's layout with 32-bit pointers, so this is a 32-bit build (x86_64-linux hosts):
        # gcc_multi for -m32 and the 32-bit SDL3 from pkgsi686Linux.
        native = (pkgs.overrideCC pkgs.stdenv pkgs.gcc_multi).mkDerivation {
          pname = "fa18-native";
          version = "0";
          src = src pkgs;
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            pkgs.python3
            pkgs.patchelf
          ];
          buildInputs = [
            pkgs.pkgsi686Linux.sdl3
          ];
          cmakeFlags = [ "-DBUILD_TESTING=OFF" "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_C_FLAGS=-m32" ];
          ninjaFlags = [ "fa18_native" ];
          installPhase = ''
            install -Dm755 native/fa18_native $out/bin/fa18_native
          '';
          postFixup = ''
            patchelf --set-interpreter /lib/ld-linux.so.2 --remove-rpath $out/bin/fa18_native
          '';
        };

        sdl3-emscripten = import ./sdl3-emscripten.nix { inherit pkgs; };

        # Browser build; SDL3 is compiled from nixpkgs' source for Emscripten.
        web = pkgs.stdenv.mkDerivation {
          pname = "fa18-web";
          version = "0";
          src = src pkgs;
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.python3
            pkgs.nodejs
            pkgs.emscripten
          ];
          dontUseCmakeConfigure = true;
          buildPhase = ''
            export HOME=$TMPDIR EM_CACHE=$TMPDIR/em-cache
            emcmake cmake -S . -B build-web -G Ninja \
              -DBUILD_TESTING=OFF \
              -DCMAKE_BUILD_TYPE=Release \
              -DSDL3_DIR=${self.packages.${pkgs.system}.sdl3-emscripten}/lib/cmake/SDL3
            cmake --build build-web --target fa18_native
          '';
          installPhase = ''
            mkdir -p $out
            cp build-web/native/fa18.html $out/
            cp build-web/native/fa18.js $out/
            cp build-web/native/fa18.wasm $out/
            cp build-web/native/fa18-pcm-worklet.js $out/
            cp build-web/native/retro-foundry.png $out/
            cp build-web/native/gamepad-controls.svg $out/
            cp build-web/native/flight-controls.js $out/
            cp build-web/native/controller-overlay.js $out/
            cp build-web/native/flightstick-controls.svg $out/
            cp build-web/native/hotas-controls.svg $out/
            cp build-web/native/flightstick-controls.png $out/
            cp build-web/native/hotas-controls.png $out/
          '';
        };
        default = self.packages.${pkgs.system}.native;
      });

      devShells = forAll (pkgs: {
        default = pkgs.mkShell.override { stdenv = pkgs.overrideCC pkgs.stdenv pkgs.gcc_multi; } {
          buildInputs = [ pkgs.pkgsi686Linux.sdl3 ];
          packages = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            pkgs.gcc_multi
            pkgs.nodejs
            pkgs.emscripten
            m68k-amigaos-gcc.packages.${pkgs.system}.default
            (pkgs.python3.withPackages (ps: [ ps.playwright ps.pyelftools ]))
          ];
          GODS_SDL3_SOURCE_DIR = pkgs.sdl3.src;
          PLAYWRIGHT_BROWSERS_PATH = pkgs.playwright-driver.browsers;
          PLAYWRIGHT_SKIP_VALIDATE_HOST_REQUIREMENTS = "true";
          shellHook = ''
            export EM_CACHE="$PWD/build/emscripten-cache"
          '';
        };
      });
    };
}
