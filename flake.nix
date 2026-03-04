{
  description = "Fluxland — a Fluxbox-inspired Wayland compositor";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.stdenv.mkDerivation {
            pname = "fluxland";
            version = "1.1.0";

            src = self;

            nativeBuildInputs = with pkgs; [
              meson
              ninja
              pkg-config
              wayland-scanner
            ];

            buildInputs = with pkgs; [
              wlroots_0_18
              wayland
              wayland-protocols
              libxkbcommon
              pango
              cairo
              libinput
              libdrm
              pixman
              libxcb
              xcb-util-wm
            ];

            mesonFlags = [
              "-Dxwayland=auto"
            ];

            meta = with pkgs.lib; {
              description = "A Fluxbox-inspired Wayland compositor";
              homepage = "https://github.com/ecliptik/fluxland";
              license = licenses.mit;
              platforms = platforms.linux;
              mainProgram = "fluxland";
            };
          };
        });

      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.default ];
          };
        });
    };
}
