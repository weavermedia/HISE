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

echo "BUILT_PRODUCTS_DIR: $built_products_dir"

# Remove the 'YES' from the end of the path if present
built_products_dir=${built_products_dir%YES}

echo "Adjusted BUILT_PRODUCTS_DIR: $built_products_dir"

# Remove trailing space if present
built_products_dir=$(echo "$built_products_dir" | sed 's/[[:space:]]*$//')

echo "Final BUILT_PRODUCTS_DIR: $built_products_dir"

# Check if the app exists with either name and rename it
app_path_standalone="$built_products_dir/HISE Standalone.app"
app_path_debug="$built_products_dir/HISE Debug.app"

if [ -d "$app_path_standalone" ]; then
    mv "$app_path_standalone" "${app_path_standalone%/*}/HISE.app"
    echo "HISE.app is ready for upload at ${app_path_standalone%/*}/HISE.app"
elif [ -d "$app_path_debug" ]; then
    mv "$app_path_debug" "${app_path_debug%/*}/HISE.app"
    echo "HISE.app is ready for upload at ${app_path_debug%/*}/HISE.app"
else
    echo "Error: Built app not found"
    echo "Contents of build directory:"
    ls -la "$standalone_folder/Builds/MacOSX/build"
    echo "Contents of Debug directory:"
    ls -la "$built_products_dir"
    exit 1
fi
