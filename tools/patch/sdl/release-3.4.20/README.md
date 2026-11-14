# SDL 3.4.20 Patches

Applied by `scripts/apple/sdl.sh`:

- `cmake.mac-catalyst.patch`: detect Catalyst from the compiler target so SDL
  builds UIKit video instead of the incompatible Cocoa backend.
- `uikit.disabled-joystick.patch`: fix tvOS remote handling when joystick support
  is disabled, without enabling the joystick subsystem.
- `uikit.visionos-availability.patch`: guard pen-roll APIs that require visionOS
  1.2, preserving the project's existing minimum OS version.
