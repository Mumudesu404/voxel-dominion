@echo off
setlocal
rem Unreal Engine path: UE_ENGINE env var wins, otherwise the local default install.
if not defined UE_ENGINE set "UE_ENGINE=D:\UE_5.7"
set "ENGINE=%UE_ENGINE%"
set "UE-LocalDataCachePath=%~dp0DerivedDataCache\FileCache"
if not exist "%ENGINE%\Engine\Binaries\Win64\UnrealEditor.exe" (
  echo UE 5.7 was not found at %ENGINE%.
  echo Set UE_ENGINE to your UE 5.7 install, e.g. setx UE_ENGINE "C:\Program Files\Epic Games\UE_5.7"
  pause
  exit /b 1
)
start "Voxel Dominion Editor" "%ENGINE%\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0test.uproject" -ddc=InstalledNoZenLocalFallback -d3d11
