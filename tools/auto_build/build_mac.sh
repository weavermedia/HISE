#!/bin/bash
set -e

cd "$(dirname "$0")"
cd ../..

standalone_folder="projects/standalone"

chmod +x "tools/Projucer/Projucer.app/Contents/MacOS/Projucer"

"tools/Projucer/Projucer.app/Contents/MacOS/Projucer" --resave "projects/standalone/HISE Standalone.jucer"

echo "Compiling Standalone App..."

# Xcode build command
xcodebuild -project "$standalone_folder/Builds/MacOSX/HISE Standalone.xcodeproj" -configuration Debug | xcpretty

echo "Build completed successfully"

# Get the BUILT_PRODUCTS_DIR from xcodebuild
built_products_dir=$(xcodebuild -project "$standalone_folder/Builds/MacOSX/HISE Standalone.xcodeproj" -configuration Debug -showBuildSettings | grep BUILT_PRODUCTS_DIR | awk '{print $3}' | sed 's/[[:space:]]*$//')

# Check if the app exists
app_path="$built_products_dir/HISE.app"

if [ -d "$app_path" ]; then
    echo "HISE.app is ready at $app_path"
else
    echo "Error: Built app not found"
    exit 1
fi
