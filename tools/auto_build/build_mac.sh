#!/bin/bash
set -e

cd "$(dirname "$0")"
cd ../..

# This is the project folder for the Standalone app
standalone_folder="projects/standalone"

chmod +x "tools/Projucer/Projucer.app/Contents/MacOS/Projucer"

"tools/Projucer/Projucer.app/Contents/MacOS/Projucer" --resave "projects/standalone/HISE Standalone.jucer"

echo "Compiling Standalone App..."

# Show build settings for Debug configuration
echo "Debug Build settings:"
xcodebuild -project "$standalone_folder/Builds/MacOSX/HISE Standalone.xcodeproj" -configuration Debug -showBuildSettings | grep BUILT_PRODUCTS_DIR

# Actual build command
xcodebuild -project "$standalone_folder/Builds/MacOSX/HISE Standalone.xcodeproj" -configuration Debug | xcpretty

echo "Build completed successfully"

# Get the BUILT_PRODUCTS_DIR from xcodebuild
built_products_dir=$(xcodebuild -project "$standalone_folder/Builds/MacOSX/HISE Standalone.xcodeproj" -configuration Debug -showBuildSettings | grep BUILT_PRODUCTS_DIR | awk '{print $3}')

# Check if the app exists and rename it
app_path="$built_products_dir/HISE Standalone.app"
if [ -d "$app_path" ]; then
    mv "$app_path" "${app_path%/*}/HISE.app"
    echo "HISE.app is ready for upload at ${app_path%/*}/HISE.app"
else
    echo "Error: Built app not found at $app_path"
    exit 1
fi
