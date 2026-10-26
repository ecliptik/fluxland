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
            version = "2.0.0";

            src = self;

            nativeBuildInputs = with pkgs; [
              meson
              ninja
              pkg-config
              wayland-scanner
            ];

            # nixpkgs' wlroots 0.20 (the Nix sandbox can't fetch the
            # subprojects a plain meson build would bundle)
            buildInputs = with pkgs; [
              wlroots_0_20
              wayland
              wayland-protocols
              libxkbcommon
              pango
              cairo
              gdk-pixbuf
              libinput
              libdrm
              pixman
              systemd
              libxcb
              xorg.xcbutilwm
              xwayland
            ];

            mesonFlags = [
              "-Dxwayland=enabled"
              "-Dwerror=false"
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
