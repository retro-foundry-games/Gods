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
        # SDL3 compiled once for Emscripten (nixpkgs' source), so rebuilds of the game reuse it.
        sdl3-emscripten = import ./sdl3-emscripten.nix { inherit pkgs; };

        # The browser build, the folder to publish: index.html (the same as gods.html), gods.js and gods.wasm (src/sdl/Makefile.web).  The page asks for the two disk images; serve it with scripts/serve_web.py
        # (the game runs on a thread: the page needs cross-origin isolation).
        web = pkgs.stdenv.mkDerivation {
          pname = "gods-web";
          version = "0";
          src = src pkgs;
          nativeBuildInputs = [
            pkgs.gnumake
            pkgs.python3
            pkgs.emscripten
          ];
          dontUseCmakeConfigure = true;
          buildPhase = ''
            export HOME=$TMPDIR EM_CACHE=$TMPDIR/em-cache
            make -f src/sdl/Makefile.web GODS_NO_NIX=1 SDL3_PREFIX=${self.packages.${pkgs.system}.sdl3-emscripten} W=$PWD/build/web
          '';
          installPhase = ''
            mkdir -p $out
            cp build/web/gods.html build/web/gods.js build/web/gods.wasm build/web/retro-foundry.png build/web/coi-serviceworker.js $out/
            if [ -f build/web/gods.worker.js ]; then cp build/web/gods.worker.js $out/; fi
            cp build/web/gods.html $out/index.html
          '';
        };
        default = self.packages.${pkgs.system}.web;
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
