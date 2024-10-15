#!/bin/bash
set -e

cd "$(dirname "$0")"
cd ../..

# This is the project folder for the Standalone app
standalone_folder="projects/standalone"

chmod +x "tools/Projucer/Projucer.app/Contents/MacOS/Projucer"

"tools/Projucer/Projucer.app/Contents/MacOS/Projucer" --resave "projects/standalone/HISE Standalone.jucer"

echo "Compiling Standalone App..."

xcodebuild -project "$standalone_folder/Builds/MacOSX/HISE Standalone.xcodeproj" -configuration "CI" | xcpretty

echo "Build completed successfully"

# Rename the app
mv "$standalone_folder/Builds/MacOSX/build/CI/HISE Standalone.app" "$standalone_folder/Builds/MacOSX/build/CI/HISE.app"

echo "HISE.app is ready for upload"
