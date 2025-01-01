{
  description = "Open source library for N64 development.";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";

  outputs = { self, nixpkgs }:
    let

      lastModifiedDate = self.lastModifiedDate or self.lastModified or "19700101";
      version = builtins.substring 0 8 lastModifiedDate;
      supportedSystems = [ "x86_64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; overlays = [ self.overlays.default ]; });

    in

    {
      overlays.default = final: prev: {
        # FIXME: Check if we can directly use nixpkgs' mips64-embedded toolchain
        toolchain = with final; stdenv.mkDerivation {
          pname = "toolchain";
          inherit version;

          src = lib.fileset.toSource {
            root = ./.;
            fileset = lib.fileset.intersection
              (lib.fileset.gitTracked ./.)
              (lib.fileset.unions [
                ./tools/build-toolchain.sh
              ]);
          };

          buildPhase =
            let
              binutils = fetchurl {
                url = "https://ftp.gnu.org/gnu/binutils/binutils-2.43.1.tar.gz";
                hash = "sha256-5MOLiT9ZCFP74namuKEmgQHjXmGEmgf27pe17Ml/v/g=";
              };
              gcc = fetchurl {
                url = "https://ftp.gnu.org/gnu/gcc/gcc-14.2.0/gcc-14.2.0.tar.gz";
                hash = "sha256-fTdtRF+TEm3FReLACG0PZHwwlKrggc23j0LOK8JecpM=";
              };
              newlib = fetchurl {
                url = "https://sourceware.org/pub/newlib/newlib-4.4.0.20231231.tar.gz";
                hash = "sha256-DBZqOeG/CVHfr81olJ/g5LbTZYCB1igvOa7vxjEPLxM=";
              };
              gmp = fetchurl {
                url = "https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.bz2";
                hash = "sha256-rCghGnz7YJuuLiyNYFjWbI/pZDT3QM9v4uR7AA0cIMs=";
              };
              mpc = fetchurl {
                url = "https://ftp.gnu.org/gnu/mpc/mpc-1.3.1.tar.gz";
                hash = "sha256-q2QkkvXPiCt0qgy3MM1BCoHtzb7IlRg86TDnBsHHWbg=";
              };
              mpfr = fetchurl {
                url = "https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.1.tar.gz";
                hash = "sha256-EWcVVSvZZshbQXxCTbG732OfU4Nus2FUnR+Nbe1ctMY=";
              };
            in
            ''
              mkdir toolchain
              pushd toolchain
              ln -sf ${binutils} binutils-2.43.1.tar.gz
              ln -sf ${gcc} gcc-14.2.0.tar.gz
              ln -sf ${newlib} newlib-4.4.0.20231231.tar.gz
              ln -sf ${gmp} gmp-6.3.0.tar.bz2
              ln -sf ${mpc} mpc-1.3.1.tar.gz
              ln -sf ${mpfr} mpfr-4.2.1.tar.gz
              popd
              export N64_INST=$out
              patchShebangs tools
              ./tools/build-toolchain.sh
            '';
          nativeBuildInputs = [
            curl
            gnugrep
            texinfo
            perl
          ];
          hardeningDisable = [ "all" ];
          dontInstall = true;
        };
        libdragon = with final; stdenv.mkDerivation {
          pname = "libdragon";
          inherit version;


          src = lib.fileset.toSource {
            root = ./.;
            fileset = lib.fileset.intersection
              (lib.fileset.gitTracked ./.)
              (lib.fileset.unions [
                ./boot
                ./include
                ./Makefile
                ./n64.mk
                ./n64.ld
                ./rsp.ld
                ./src
              ]);
          };
          preBuild = ''
            export N64_GCCPREFIX=${toolchain}
            export N64_INST=$out
          '';
          nativeBuildInputs = [
            toolchain
          ];
          hardeningDisable = [ "all" ];
        };
        tools = with final; stdenv.mkDerivation {
          pname = "tools";
          inherit version;

          src = lib.fileset.toSource {
            root = ./.;
            fileset = lib.fileset.intersection
              (lib.fileset.gitTracked ./.)
              (lib.fileset.unions [
                ./include
                ./src
                ./tools
              ]);
          };
          preBuild = ''
            export N64_GCCPREFIX=${toolchain}
            export N64_INST=$out
            cd tools
          '';
          nativeBuildInputs = [
            toolchain
          ];
          hardeningDisable = [ "all" ];
        };
        examples = with final; stdenv.mkDerivation {
          pname = "examples";
          inherit version;
          preBuild = ''
            export N64_INST=${symlinkJoin { name = "myexample"; paths = [ toolchain tools libdragon ];  }}
            cd examples
          '';
          src = lib.fileset.toSource {
            root = ./.;
            fileset = lib.fileset.intersection
              (lib.fileset.gitTracked ./.)
              (lib.fileset.unions [
                ./src
                ./examples
              ]);
          };
          buildInputs = [
            libdragon
          ];
          installPhase = ''
            mkdir -p $out/share
            cp */*.z64 $out/share
          '';
          nativeBuildInputs = [
            toolchain
          ];
        };

      };

      packages = forAllSystems (system:
        {
          inherit (nixpkgsFor.${system}) libdragon tools examples;
          default = nixpkgsFor.${system}.libdragon;
        });

      checks = forAllSystems
        (system:
          with nixpkgsFor.${system};

          {
            inherit (self.packages.${system}) libdragon;
          }

          // lib.optionalAttrs stdenv.isLinux {
            vmTest =
              with import (nixpkgs + "/nixos/lib/testing-python.nix")
                {
                  inherit system;
                };
              makeTest {
                enableOCR = true;
                nodes = {
                  client = { pkgs, ... }: {
                    imports = [
                      (nixpkgs + "/nixos/tests/common/x11.nix")
                    ];
                    config = {
                      environment.systemPackages = [
                        pkgs.ares
                      ];
                    };
                  };
                };
                name = "ares-runs-examples";
                testScript =
                  ''
                    start_all()
                    client.wait_for_unit("multi-user.target")
                    client.succeed("""ares --setting "Audio/Driver=None" --setting "Paths/Saves=/tmp" --setting "Video/Driver=OpenGL 3.2" ${self.packages.${system}.examples}/share/audioplayer.z64 >&2 &""")
                    client.wait_for_text("Module Audio Player")
                    client.screenshot("audioplayer")
                  '';
              };
          }
        );

    };
}
