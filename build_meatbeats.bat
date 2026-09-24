@echo off
setlocal enabledelayedexpansion
REM
REM Build the HISE Standalone IDE app from the meatbeats branch (Windows).
REM
REM Usage:
REM   build_meatbeats.bat [CONFIG]
REM
REM   CONFIG  Solution configuration to build. Defaults to "Debug".
REM           Valid values: Debug, Release, CI, Minimal, DebugWithFaust, ReleaseWithFaust.
REM
REM Notes:
REM   - Hard-fails if the current branch is not "meatbeats".
REM   - Unpacks tools\SDK\sdk.zip (ASIO/VST3 SDKs) if it has not been already.
REM   - Resaves the .jucer with Projucer first, which regenerates the
REM     untracked AppConfig.h (avoids the recurring "AppConfig.h not found").
REM   - Locates MSBuild via vswhere and picks the VS2026 or VS2022 solution
REM     to match the newest installed Visual Studio.
REM   - Builds standalone only (the IDE/backend), x64.
REM   - Does NOT touch git state and does NOT run unit tests.

cd /d "%~dp0"
set "repo_root=%CD%"

set "config=%~1"
if "%config%"=="" set "config=Debug"
set "platform=x64"

set "required_branch=meatbeats"
set "standalone_folder=projects\standalone"
set "jucer_file=%standalone_folder%\HISE Standalone.jucer"
set "projucer=JUCE\projucer\Projucer.exe"
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

REM --- Branch guard (hard fail) -------------------------------------------------
set "current_branch="
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD') do set "current_branch=%%b"
if not "%current_branch%"=="%required_branch%" (
  echo Error: current branch is '%current_branch%', expected '%required_branch%'. 1>&2
  echo Checkout the meatbeats branch before building. 1>&2
  exit /b 1
)

REM --- Sanity checks ------------------------------------------------------------
if not exist "%projucer%" (
  echo Error: Projucer not found at %projucer% 1>&2
  echo Make sure the JUCE submodule is checked out. 1>&2
  exit /b 1
)

if not exist "%jucer_file%" (
  echo Error: jucer file not found at %jucer_file% 1>&2
  exit /b 1
)

if not exist "%vswhere%" (
  echo Error: vswhere.exe not found - is Visual Studio installed? 1>&2
  exit /b 1
)

REM --- Locate MSBuild and pick the matching solution ----------------------------
set "msbuild="
set "vs_version="
for /f "usebackq delims=" %%m in (`"%vswhere%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "msbuild=%%m"
for /f "usebackq delims=" %%v in (`"%vswhere%" -latest -products * -requires Microsoft.Component.MSBuild -property installationVersion`) do set "vs_version=%%v"

if not defined msbuild (
  echo Error: MSBuild not found. Install the "Desktop development with C++" workload. 1>&2
  exit /b 1
)

set "vs_major=%vs_version:~0,2%"
if "%vs_major%"=="18" (set "vs_folder=VisualStudio2026") else (set "vs_folder=VisualStudio2022")
set "solution=%standalone_folder%\Builds\%vs_folder%\HISE Standalone.sln"

if not exist "%solution%" (
  echo Error: solution not found at %solution% 1>&2
  exit /b 1
)

REM --- Third party SDKs ---------------------------------------------------------
if not exist "tools\SDK\ASIOSDK2.3" (
  echo Unpacking tools\SDK\sdk.zip...
  pushd tools\SDK
  tar -xf sdk.zip
  set "tar_result=!errorlevel!"
  popd
  if not "!tar_result!"=="0" (
    echo Error: could not unpack tools\SDK\sdk.zip 1>&2
    exit /b 1
  )
)

REM --- Resave (regenerates AppConfig.h and the VS project) ----------------------
echo Resaving Projucer project (regenerates AppConfig.h)...
"%projucer%" --resave "%jucer_file%"
if errorlevel 1 (
  echo Error: Projucer resave failed. 1>&2
  exit /b 1
)

REM --- Build -------------------------------------------------------------------
echo Building HISE Standalone [%config% / %platform%] using %vs_folder%...
echo Visual Studio version: %vs_version%
"%msbuild%" "%solution%" /t:Build /p:Configuration="%config%";Platform=%platform% /m /v:m
if errorlevel 1 (
  echo Error: build failed. 1>&2
  exit /b 1
)

REM --- Verify output -----------------------------------------------------------
REM The exe name follows the per-config targetName in the .jucer (Debug gives
REM "HISE Debug.exe", Release gives "HISE.exe"), so glob rather than hardcode.
set "build_dir=%standalone_folder%\Builds\%vs_folder%\%platform%\%config%\App"
set "exe_path="
for %%f in ("%build_dir%\*.exe") do set "exe_path=%%~f"

if defined exe_path (
  echo Build completed successfully.
  echo App is at %exe_path%
) else (
  echo Error: built exe not found in %build_dir% 1>&2
  exit /b 1
)

endlocal
