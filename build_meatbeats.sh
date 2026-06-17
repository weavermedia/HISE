#!/bin/bash
#
# Build the HISE Standalone IDE app from the meatbeats branch.
#
# Usage:
#   ./build_meatbeats.sh [CONFIG]
#
#   CONFIG  Xcode configuration to build. Defaults to "Debug".
#           Valid values: Debug, Release, CI, "Minimal Build".
#
# Notes:
#   - Hard-fails if the current branch is not "meatbeats".
#   - Resaves the .jucer with Projucer first, which regenerates the
#     untracked AppConfig.h (avoids the recurring "AppConfig.h not found").
#   - Builds standalone only (the IDE/backend), arm64.
#   - Does NOT touch git state and does NOT run unit tests.

set -euo pipefail

# Move to repo root (this script lives at the repo root).
cd "$(dirname "$0")"
repo_root="$PWD"

config="${1:-Debug}"
arch="arm64"

required_branch="meatbeats"
standalone_folder="projects/standalone"
jucer_file="$standalone_folder/HISE Standalone.jucer"
xcodeproj="$standalone_folder/Builds/MacOSX/HISE Standalone.xcodeproj"
projucer="JUCE/projucer/Projucer.app/Contents/MacOS/Projucer"
xcbeautify="tools/projucer/xcbeautify"

# --- Branch guard (hard fail) -------------------------------------------------
current_branch="$(git rev-parse --abbrev-ref HEAD)"
if [ "$current_branch" != "$required_branch" ]; then
  echo "Error: current branch is '$current_branch', expected '$required_branch'." >&2
  echo "Checkout the meatbeats branch before building." >&2
  exit 1
fi

# --- Sanity checks ------------------------------------------------------------
if [ ! -x "$projucer" ]; then
  echo "Error: Projucer not found at $projucer" >&2
  echo "Make sure the JUCE submodule is checked out." >&2
  exit 1
fi

if [ ! -f "$jucer_file" ]; then
  echo "Error: jucer file not found at $jucer_file" >&2
  exit 1
fi

# --- Resave (regenerates AppConfig.h and the Xcode project) -------------------
echo "Resaving Projucer project (regenerates AppConfig.h)..."
"$projucer" --resave "$jucer_file"

# --- Build -------------------------------------------------------------------
echo "Building HISE Standalone [$config / $arch]..."
echo "macOS version: $(sw_vers -productVersion)"
echo "Xcode version: $(xcodebuild -version | head -n 1)"

# Keep xcodebuild's exit code even when piped through the beautifier.
set -o pipefail
if [ -x "$xcbeautify" ]; then
  xcodebuild -project "$xcodeproj" -configuration "$config" -arch "$arch" | "$xcbeautify"
else
  echo "Note: $xcbeautify not found - using verbose build output."
  xcodebuild -project "$xcodeproj" -configuration "$config" -arch "$arch"
fi

# --- Verify output -----------------------------------------------------------
app_path="$standalone_folder/Builds/MacOSX/build/$config/HISE.app"
if [ -d "$app_path" ]; then
  echo "Build completed successfully."
  echo "HISE.app is at $repo_root/$app_path"
else
  echo "Error: built app not found at $app_path" >&2
  exit 1
fi
