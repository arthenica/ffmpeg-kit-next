Pod::Spec.new do |s|
  s.name             = 'ffmpeg_kit_next_flutter'
  s.version          = '8.1.0'
  s.summary          = 'FFmpeg Kit Next for Flutter'
  s.description      = 'A Flutter plugin for running FFmpeg and FFprobe commands.'
  s.homepage         = 'https://github.com/arthenica/ffmpeg-kit-next'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'ARTHENICA' => 'open-source@arthenica.com' }

  s.platform              = :osx
  s.requires_arc          = true
  s.static_framework      = true
  s.osx.deployment_target = '10.15'

  s.source              = { :path => '.' }
  s.source_files        = 'ffmpeg_kit_next_flutter/Sources/ffmpeg_kit_next_flutter/**/*.{h,m}'
  s.public_header_files = 'ffmpeg_kit_next_flutter/Sources/ffmpeg_kit_next_flutter/include/**/*.h'

  s.dependency          'FlutterMacOS'
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES' }

  # FFmpegKit native binaries are built locally with nix-macos.sh and copied into
  # ffmpeg_kit_next_flutter/Frameworks by copy_local_binaries.sh. No remote pod is
  # referenced, so the selected package variant (min/https/full/gpl, LTS or not) is
  # whatever was built.
  s.vendored_frameworks = 'ffmpeg_kit_next_flutter/Frameworks/*.xcframework'
end
