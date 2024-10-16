#!/bin/bash
set -e

cd "$(dirname "$0")"
cd ../..

standalone_folder="projects/standalone"

chmod +x "tools/Projucer/Projucer.app/Contents/MacOS/Projucer"

"tools/Projucer/Projucer.app/Contents/MacOS/Projucer" --resave "projects/standalone/HISE Standalone.jucer"

echo "Compiling Standalone App..."

xcodebuild -project "$standalone_folder/Builds/MacOSX/HISE Standalone.xcodeproj" -configuration Debug | xcpretty

echo "Build completed successfully"

# Check if the app exists at the right path
app_path="projects/standalone/Builds/MacOSX/build/Debug/HISE Debug.app"

if [ -d "$app_path" ]; then
  echo "HISE Debug.app is ready for upload at $app_path"

  # Zip the app
  zip_path="${app_path%.app}.zip"
  echo "Zipping the app..."
  zip -r "$zip_path" "$app_path"

  if [ $? -eq 0 ]; then
    echo "HISE Debug.app has been successfully zipped to $zip_path"
  else
    echo "Error: Failed to zip the app"
    exit 1
  fi
else
  echo "Error: Built app not found"
  exit 1
fi
