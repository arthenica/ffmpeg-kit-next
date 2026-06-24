require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = package["name"]
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platform          = :ios, "12.1"
  s.requires_arc      = true
  s.static_framework  = true

  s.source       = { :git => "https://github.com/arthenica/ffmpeg-kit-next.git", :branch => "main" }

  s.source_files        = '**/FFmpegKitReactNativeModule.{m,mm}',
                          '**/FFmpegKitReactNativeModule.h'
  s.vendored_frameworks = "ios/Frameworks/*.xcframework"

  # New Architecture: install_modules_dependencies wires React-Core, the new-arch
  # pods, and runs codegen for this module. Independent of vendored_frameworks.
  if respond_to?(:install_modules_dependencies, true)
    install_modules_dependencies(s)
  else
    s.dependency "React-Core"
  end

end
