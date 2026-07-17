{
  description = "FFmpegKitNext build toolchain";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    nixpkgsWeb.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs, nixpkgsWeb }:
    let
      systems = [
        "aarch64-darwin"
        "aarch64-linux"
        "x86_64-darwin"
        "x86_64-linux"
      ];

      forAllSystems = f:
        nixpkgs.lib.genAttrs systems
          (system:
            f system
            (import nixpkgs {
              inherit system;
              config = {
                allowUnfree = true;
                android_sdk.accept_license = true;
              };
            })
            (import nixpkgsWeb {
              inherit system;
              config = {
                allowUnfree = true;
              };
            }));

      android = {
        platformVersion = "34";
        buildToolsVersion = "35.0.0";
        cmdLineToolsVersion = "13.0";
        cmakeVersion = "3.22.1";
        ndkVersion = "27.3.13750724";
      };

      androidComposition = pkgs: ndkVersion:
        pkgs.androidenv.composeAndroidPackages {
          cmdLineToolsVersion = android.cmdLineToolsVersion;
          platformToolsVersion = "35.0.2";
          buildToolsVersions = [ android.buildToolsVersion ];
          platformVersions = [ android.platformVersion ];
          includeCmake = true;
          cmakeVersions = [ android.cmakeVersion ];
          includeNDK = true;
          ndkVersions = [ ndkVersion ];
          includeEmulator = false;
          includeSources = false;
          includeSystemImages = false;
        };

      pkgConfigPackages = pkgs: with pkgs; [
        zlib
      ];

      pkgConfigLibdirFor = pkgs: packages:
        pkgs.lib.concatStringsSep ":" [
          (pkgs.lib.makeSearchPathOutput "dev" "lib/pkgconfig" packages)
          (pkgs.lib.makeSearchPathOutput "dev" "share/pkgconfig" packages)
        ];

      pkgConfigLibdir = pkgs: pkgConfigLibdirFor pkgs (pkgConfigPackages pkgs);

      # THE ARCHITECTURE NAME USED IN FFMPEG-KIT BUILD SCRIPTS AND DOCUMENTATION
      linuxArchName = pkgs:
        if pkgs.stdenv.hostPlatform.isAarch64 then "arm64" else "x86_64";

      # THE DEBIAN MULTIARCH TUPLE, E.G. aarch64-linux-gnu OR x86_64-linux-gnu
      linuxMultiarchTuple = pkgs:
        "${pkgs.stdenv.hostPlatform.parsed.cpu.name}-linux-gnu";

      linuxSystemPkgConfigLibdir = pkgs:
        pkgs.lib.concatStringsSep ":" [
          "/usr/local/lib/pkgconfig"
          "/usr/local/lib/${linuxMultiarchTuple pkgs}/pkgconfig"
          "/usr/local/share/pkgconfig"
          "/usr/lib/pkgconfig"
          "/usr/lib64/pkgconfig"
          "/usr/lib/${linuxMultiarchTuple pkgs}/pkgconfig"
          "/usr/lib/${pkgs.stdenv.hostPlatform.config}/pkgconfig"
          "/usr/share/pkgconfig"
        ];

      toolchainShellHook = pkgs: ''
        export BISON="${pkgs.bison}/bin/bison"
        export MESON="${pkgs.meson}/bin/meson"
        export SED="${pkgs.gnused}/bin/sed"

        export FFMPEG_KIT_NIX_HOST_SDKROOT="''${SDKROOT-}"
        unset SDKROOT
        unset MACOSX_DEPLOYMENT_TARGET

        echo -e "INFO: Using BISON at $BISON\n" >> "$PWD/build.log"
        echo -e "INFO: Using MESON at $MESON\n" >> "$PWD/build.log"
        echo -e "INFO: Using SED at $SED\n" >> "$PWD/build.log"
      '';

      pkgConfigShellHookWith = pkgs: pkgConfigLibdir: ''
        ${toolchainShellHook pkgs}

        export FFMPEG_KIT_NIX_PKG_CONFIG_LIBDIR="${pkgConfigLibdir}"
        export PKG_CONFIG_LIBDIR="$FFMPEG_KIT_NIX_PKG_CONFIG_LIBDIR"
        unset PKG_CONFIG_PATH

        echo -e "INFO: Using PKG_CONFIG_LIBDIR at $PKG_CONFIG_LIBDIR\n" >> "$PWD/build.log"
      '';

      pkgConfigShellHook = pkgs: pkgConfigShellHookWith pkgs (pkgConfigLibdir pkgs);

      linuxPkgConfigShellHook = pkgs: ''
        ${toolchainShellHook pkgs}

        export FFMPEG_KIT_SYSTEM_PKG_CONFIG_LIBDIR="${linuxSystemPkgConfigLibdir pkgs}"
        unset FFMPEG_KIT_NIX_PKG_CONFIG_LIBDIR
        unset PKG_CONFIG_LIBDIR
        unset PKG_CONFIG_PATH

        echo -e "INFO: Using system PKG_CONFIG_LIBDIR at $FFMPEG_KIT_SYSTEM_PKG_CONFIG_LIBDIR\n" >> "$PWD/build.log"
      '';

      commonToolPackages = pkgs: with pkgs; [
        bash
        git
        curl
        wget
        zip
        unzip
        gnutar
        gzip
        xz
        cmake
        bison
        meson
        ninja
        pkg-config
        autoconf
        automake
        libtool
        gnused
        nasm
        yasm
        gettext
        gperf
        python3
        perl
        ruby
      ];

      commonPackages = pkgs: commonToolPackages pkgs ++ pkgConfigPackages pkgs;

      androidPackages = pkgs: with pkgs; commonPackages pkgs ++ [
        autogen
        coreutils
        doxygen
        file
        findutils
        gnumake
        gnugrep
        groff
        gtk-doc
        jdk17
        libtasn1
        patch
        ragel
        texinfo
        which
      ] ++ pkgs.lib.optionals pkgs.stdenv.hostPlatform.isDarwin [
        clang
      ] ++ pkgs.lib.optionals pkgs.stdenv.hostPlatform.isLinux [
        gcc
      ];

      linuxToolchainPackages = pkgs: with pkgs; commonToolPackages pkgs ++ [
        autogen
        coreutils
        doxygen
        file
        findutils
        gawk
        gcc
        gnumake
        gnugrep
        groff
        gtk-doc
        patch
        ragel
        rsync
        tcl
        texinfo
        which
        llvmPackages.clang
        llvmPackages.llvm
        llvmPackages.lld
        llvmPackages.libclang
      ];

      webToolchainPackages = pkgs: with pkgs; [
        bash
        binaryen
        bison
        cmake
        coreutils
        curl
        emscripten
        file
        findutils
        gawk
        gettext
        git
        gperf
        gnumake
        gnugrep
        gnused
        gnutar
        gzip
        libtool
        m4
        meson
        ninja
        nodejs
        perl
        pkg-config
        python3
        rsync
        ruby
        texinfo
        unzip
        wget
        which
        xz
        zip
        autoconf
        automake
      ];

      xcodeMinCheck = minMajor: ''
        # Respect Apple's normal precedence: an explicitly-set DEVELOPER_DIR wins, then the
        # Xcode currently selected via xcode-select (works regardless of where Xcode is
        # installed or what it's named — versioned installs, /Applications/Xcode.app not
        # existing or being a stale/dangling symlink, etc), then the previous hardcoded
        # default as a last resort.
        if [ -z "$DEVELOPER_DIR" ] || [ ! -x "$DEVELOPER_DIR/usr/bin/xcodebuild" ]; then
          DEVELOPER_DIR="$(/usr/bin/xcode-select -p 2>/dev/null || true)"
        fi
        if [ -z "$DEVELOPER_DIR" ] || [ ! -x "$DEVELOPER_DIR/usr/bin/xcodebuild" ]; then
          DEVELOPER_DIR="/Applications/Xcode.app/Contents/Developer"
        fi
        export DEVELOPER_DIR

        if [ ! -x "$DEVELOPER_DIR/usr/bin/xcodebuild" ]; then
          echo "Error: Xcode is required at $DEVELOPER_DIR"
          exit 1
        fi

        XCODE_VERSION=$("$DEVELOPER_DIR/usr/bin/xcodebuild" -version | head -n1 | awk '{print $2}')
        XCODE_MAJOR=$(echo "$XCODE_VERSION" | cut -d. -f1)

        if [ "$XCODE_MAJOR" -lt "${minMajor}" ]; then
          echo "Error: Xcode ${minMajor}.x or newer is required."
          echo "Detected Xcode: $XCODE_VERSION"
          echo -e "INFO: Active path: $DEVELOPER_DIR\n" >> "$PWD/build.log"
          exit 1
        fi

        export CC="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
        export CXX="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++"
        export AR="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/ar"
        export RANLIB="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/ranlib"
        export STRIP="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip"

        echo "Using Xcode $XCODE_VERSION"
        echo -e "INFO: Active path: $DEVELOPER_DIR\n" >> "$PWD/build.log"
        echo -e "INFO: CC: $CC\n" >> "$PWD/build.log"
      '';

      androidShell = system: pkgs: profileName: ndkVersion:
        let
          composition = androidComposition pkgs ndkVersion;
          androidSdk = composition.androidsdk;
          androidSdkRoot = "${androidSdk}/libexec/android-sdk";
          androidNdkRoot = "${androidSdkRoot}/ndk/${ndkVersion}";
          androidCmakeRoot = "${androidSdkRoot}/cmake/${android.cmakeVersion}";
          androidPath = pkgs.lib.makeBinPath ((androidPackages pkgs) ++ [ androidSdk ]);
        in
        pkgs.mkShellNoCC {
          packages = (androidPackages pkgs) ++ [
            androidSdk
          ];

          shellHook = ''
            export PATH="${androidCmakeRoot}/bin:${androidSdkRoot}/platform-tools:${androidPath}:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
            export ACLOCAL_PATH="${pkgs.gettext}/share/aclocal:$ACLOCAL_PATH"
            ${pkgConfigShellHook pkgs}

            unset CFLAGS
            unset CXXFLAGS
            unset CPPFLAGS
            unset LDFLAGS
            unset CC
            unset CXX
            unset AR
            unset AS
            unset LD
            unset NM
            unset RANLIB
            unset STRIP
            unset CMAKE_INCLUDE_PATH
            unset CMAKE_LIBRARY_PATH
            unset CMAKE_PREFIX_PATH
            unset NIXPKGS_CMAKE_PREFIX_PATH
            unset CMAKE_OSX_ARCHITECTURES
            unset CMAKE_OSX_DEPLOYMENT_TARGET
            unset CMAKE_OSX_SYSROOT
            unset NIX_CFLAGS_COMPILE
            unset NIX_CFLAGS_COMPILE_FOR_BUILD
            unset NIX_LDFLAGS
            unset NIX_LDFLAGS_FOR_BUILD

            export JAVA_HOME="${pkgs.jdk17.home}"
            export ANDROID_HOME="${androidSdkRoot}"
            export ANDROID_SDK_ROOT="$ANDROID_HOME"
            export ANDROID_NDK_ROOT="${androidNdkRoot}"
            export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
            export ANDROID_NDK="$ANDROID_NDK_ROOT"
            export CMAKE="${androidCmakeRoot}/bin/cmake"

            export FFMPEG_KIT_NIX_ANDROID_PLATFORM="${android.platformVersion}"
            export FFMPEG_KIT_NIX_ANDROID_BUILD_TOOLS="${android.buildToolsVersion}"
            export FFMPEG_KIT_NIX_ANDROID_CMAKE="${android.cmakeVersion}"
            export FFMPEG_KIT_NIX_ANDROID_NDK="${ndkVersion}"

            export GRADLE_USER_HOME="''${GRADLE_USER_HOME:-$PWD/.gradle}"

            if [ ! -f "$ANDROID_NDK_ROOT/source.properties" ]; then
              echo "Error: Android NDK was not found at $ANDROID_NDK_ROOT"
              exit 1
            fi

            case "$(uname -s)-$(uname -m)" in
              Darwin-arm64)
                if [ -d "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-aarch64" ]; then
                  export ANDROID_TOOLCHAIN="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-aarch64"
                fi
                ;;
            esac

            echo "FFmpegKit ${profileName} environment loaded for ${system}"
            echo -e "INFO: Using ANDROID_SDK_ROOT at $ANDROID_SDK_ROOT\n" >> "$PWD/build.log"
            echo -e "INFO: Using ANDROID_NDK_ROOT at $ANDROID_NDK_ROOT\n" >> "$PWD/build.log"
            echo -e "INFO: Using JAVA_HOME at $JAVA_HOME\n" >> "$PWD/build.log"
          '';
        };

      linuxToolchainShell = system: pkgs:
        pkgs.mkShellNoCC {
          packages = linuxToolchainPackages pkgs;

          shellHook = ''
            export TCLSH="${pkgs.tcl}/bin/tclsh"
            export PATH="${pkgs.lib.makeBinPath (linuxToolchainPackages pkgs)}:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
            export ACLOCAL_PATH="${pkgs.gettext}/share/aclocal:$ACLOCAL_PATH"
            export FFMPEG_KIT_NIX_GLIBC_VERSION="${pkgs.glibc.version}"
            ${linuxPkgConfigShellHook pkgs}

            echo "FFmpegKit Linux ${linuxArchName pkgs} glibc ${pkgs.glibc.version} toolchain environment loaded for ${system}"
          '';
        };

      webToolchainShell = system: pkgs:
        pkgs.mkShellNoCC {
          packages = webToolchainPackages pkgs;

          shellHook = ''
            export PATH="${pkgs.lib.makeBinPath (webToolchainPackages pkgs)}:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
            export ACLOCAL_PATH="${pkgs.gettext}/share/aclocal:$ACLOCAL_PATH"
            export FFMPEG_KIT_WEB_TARGET="wasm32-unknown-emscripten"
            export FFMPEG_KIT_WEB_ARCH="wasm32"
            export FFMPEG_KIT_WEB_NIXPKGS="github:NixOS/nixpkgs/nixos-26.05"
            export EM_CACHE="''${EM_CACHE:-$PWD/.tmp/emscripten-cache}"
            ${toolchainShellHook pkgs}

            unset CFLAGS
            unset CXXFLAGS
            unset CPPFLAGS
            unset LDFLAGS
            unset PKG_CONFIG_PATH
            unset PKG_CONFIG_LIBDIR
            unset CC
            unset CXX
            unset AR
            unset AS
            unset LD
            unset NM
            unset RANLIB
            unset STRIP
            unset CMAKE_INCLUDE_PATH
            unset CMAKE_LIBRARY_PATH
            unset CMAKE_PREFIX_PATH
            unset NIXPKGS_CMAKE_PREFIX_PATH
            unset NIX_CFLAGS_COMPILE
            unset NIX_CFLAGS_COMPILE_FOR_BUILD
            unset NIX_LDFLAGS
            unset NIX_LDFLAGS_FOR_BUILD

            mkdir -p "$EM_CACHE"

            # SEED THE WRITABLE CACHE FROM THE PACKAGED EMSCRIPTEN CACHE SO THE FIRST
            # BUILD DOES NOT HAVE TO REBUILD THE WHOLE SYSROOT OFFLINE.
            if [ -z "$(ls -A "$EM_CACHE" 2>/dev/null)" ]; then
              cp -r --no-preserve=mode "${pkgs.emscripten}/share/emscripten/cache/." "$EM_CACHE"/ 2>/dev/null || true
            fi

            echo "FFmpegKit Web wasm32 Emscripten environment loaded for ${system}"
            echo -e "INFO: Using Emscripten at $(command -v emcc)\n" >> "$PWD/build.log"
            echo -e "INFO: Using EM_CACHE at $EM_CACHE\n" >> "$PWD/build.log"
          '';
        };
    in
    {
      devShells = forAllSystems (system: pkgs: webPkgs:
        let
          androidR27dShell = androidShell system pkgs "Android NDK r27d" android.ndkVersion;
          linuxToolchainDevShell = linuxToolchainShell system pkgs;
          webWasm32EmscriptenShell = webToolchainShell system webPkgs;
          linuxGlibcProfileName =
            if pkgs.stdenv.hostPlatform.isLinux
            then "linux-${linuxArchName pkgs}-glibc-${pkgs.lib.replaceStrings [ "." ] [ "_" ] (builtins.head (pkgs.lib.splitString "-" pkgs.glibc.version))}"
            else null;
          xcode26Shell = pkgs.mkShellNoCC {
            packages = commonPackages pkgs;

            shellHook = ''
              export PATH="${pkgs.lib.makeBinPath (commonPackages pkgs)}:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
              export ACLOCAL_PATH="${pkgs.gettext}/share/aclocal:$ACLOCAL_PATH"
              ${pkgConfigShellHook pkgs}

              echo "FFmpegKit Xcode 26+ environment loaded for ${system}"
              ${xcodeMinCheck "26"}
            '';
          };
        in
        {
          default =
            if pkgs.stdenv.hostPlatform.isLinux
            then linuxToolchainDevShell
            else xcode26Shell;

          xcode26 = xcode26Shell;

          "android-r27d" = androidR27dShell;
          "web-wasm32-emscripten" = webWasm32EmscriptenShell;
        } // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
          "${linuxGlibcProfileName}" = linuxToolchainDevShell;
        });
    };
}
