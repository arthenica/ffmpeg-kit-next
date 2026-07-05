# ffmpeg-kit Protocol Host Tests

These tests are intentionally host-only. They are invoked by platform
`ffmpeg.sh` scripts after custom ffmpeg-kit protocol patches are applied to the
FFmpeg source tree.

Run a platform wrapper from the repository root:

```sh
scripts/apple/ffmpeg-kit-protocols-test.sh "$(pwd)" "$(pwd)/src/ffmpeg"
scripts/android/ffmpeg-kit-protocols-test.sh "$(pwd)" "$(pwd)/src/ffmpeg"
scripts/linux/ffmpeg-kit-protocols-test.sh "$(pwd)" "$(pwd)/src/ffmpeg"
```

The FFmpeg source directory passed to a wrapper must already contain that
platform's custom protocol patches.

Each wrapper deletes its previous output, compiles
`tools/protocols/test/ffkit_protocol_id_test.c` with a host compiler, and writes
only under `.tmp/ffkitprotocols/<platform>/tests`. It does not create, modify,
or package any `ffmpeg-kit` library artifact.

When running from a cross-compile script, use `HOST_CC` if the default host
compiler should be overridden. The wrappers intentionally ignore `CC`, which may
point to a target compiler inside platform build scripts.
