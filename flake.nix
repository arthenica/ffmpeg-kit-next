{
  description = "FFmpegKitNext build toolchain";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [
        "aarch64-darwin"
        "x86_64-darwin"
        "x86_64-linux"
      ];

      forAllSystems = f:
        nixpkgs.lib.genAttrs systems
          (system:
            f system (import nixpkgs {
              inherit system;
              config = {
                allowUnfree = true;
                android_sdk.accept_license = true;
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
      ] ++ pkgs.lib.optionals pkgs.stdenv.hostPlatform.isDarwin [
        libiconv
      ];

      linuxFullPkgConfigPackages = pkgs:
        pkgConfigPackages pkgs ++ (with pkgs; [
          alsa-lib
          expat
          fftw
          fontconfig
          freetype
          fribidi
          giflib
          gmp
          gnutls
          harfbuzz
          lame
          leptonica
          libass
          libdrm
          libjpeg
          libogg
          libopus
          libpng
          libsamplerate
          libtasn1
          libtheora
          libtiff
          libunistring
          libva
          libvorbis
          libvpx
          libwebp
          libxml2
          nettle
          ocl-icd
          opencore-amr
          opencl-headers
          p11-kit
          rubberband
          SDL2
          shine
          snappy
          soxr
          speex
          tesseract
          twolame
          v4l-utils
          vamp-plugin-sdk
          vid-stab
          vo-amrwbenc
          x265
          xvidcore
        ]);

      pkgConfigLibdirFor = pkgs: packages:
        pkgs.lib.concatStringsSep ":" [
          (pkgs.lib.makeSearchPathOutput "dev" "lib/pkgconfig" packages)
          (pkgs.lib.makeSearchPathOutput "dev" "share/pkgconfig" packages)
        ];

      pkgConfigLibdir = pkgs: pkgConfigLibdirFor pkgs (pkgConfigPackages pkgs);

      linuxFullPkgConfigLibdir = pkgs: pkgConfigLibdirFor pkgs (linuxFullPkgConfigPackages pkgs);

      pkgConfigShellHookWith = pkgs: pkgConfigLibdir: ''
        export BISON="${pkgs.bison}/bin/bison"
        export MESON="${pkgs.meson}/bin/meson"
        export SED="${pkgs.gnused}/bin/sed"

        export FFMPEG_KIT_NIX_PKG_CONFIG_LIBDIR="${pkgConfigLibdir}"
        export PKG_CONFIG_LIBDIR="$FFMPEG_KIT_NIX_PKG_CONFIG_LIBDIR"
        unset PKG_CONFIG_PATH

        export FFMPEG_KIT_NIX_HOST_SDKROOT="''${SDKROOT-}"
        unset SDKROOT
        unset MACOSX_DEPLOYMENT_TARGET

        echo -e "INFO: Using BISON at $BISON\n" >> "$PWD/build.log"
        echo -e "INFO: Using MESON at $MESON\n" >> "$PWD/build.log"
        echo -e "INFO: Using SED at $SED\n" >> "$PWD/build.log"
        echo -e "INFO: Using PKG_CONFIG_LIBDIR at $PKG_CONFIG_LIBDIR\n" >> "$PWD/build.log"
      '';

      pkgConfigShellHook = pkgs: pkgConfigShellHookWith pkgs (pkgConfigLibdir pkgs);

      linuxFullPkgConfigShellHook = pkgs: pkgConfigShellHookWith pkgs (linuxFullPkgConfigLibdir pkgs);

      commonPackages = pkgs: with pkgs; [
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
        libiconv
        python3
        perl
        ruby
      ] ++ pkgConfigPackages pkgs;

      androidPackages = pkgs: with pkgs; commonPackages pkgs ++ [
        autogen
        coreutils
        doxygen
        file
        findutils
        gnumake
        gnugrep
        gperf
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

      linuxFullPackages = pkgs: with pkgs; commonPackages pkgs ++ [
        autogen
        coreutils
        doxygen
        file
        findutils
        gawk
        gcc
        gnumake
        gnugrep
        gperf
        groff
        gtk-doc
        libtasn1
        patch
        ragel
        rapidjson
        rsync
        tcl
        texinfo
        which
        llvmPackages.clang
        llvmPackages.llvm
        llvmPackages.lld
        llvmPackages.libclang
      ] ++ linuxFullPkgConfigPackages pkgs;

      xcodeMinCheck = minMajor: ''
        export DEVELOPER_DIR="/Applications/Xcode.app/Contents/Developer"

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

      linuxFullShell = system: pkgs:
        pkgs.mkShellNoCC {
          packages = linuxFullPackages pkgs;

          shellHook = ''
            export TCLSH="${pkgs.tcl}/bin/tclsh"
            export PATH="${pkgs.lib.makeBinPath (linuxFullPackages pkgs)}:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
            export ACLOCAL_PATH="${pkgs.gettext}/share/aclocal:$ACLOCAL_PATH"
            ${linuxFullPkgConfigShellHook pkgs}

            echo "FFmpegKit Linux full environment loaded for ${system}"
          '';
        };
    in
    {
      devShells = forAllSystems (system: pkgs:
        let
          androidR27dShell = androidShell system pkgs "Android NDK r27d" android.ndkVersion;
          linuxFullDevShell = linuxFullShell system pkgs;
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
            then linuxFullDevShell
            else xcode26Shell;

          xcode26 = xcode26Shell;

          "android-r27d" = androidR27dShell;
        } // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
          "linux-full" = linuxFullDevShell;
        });
    };
}
