@echo off
setlocal
rem Unreal Engine path: UE_ENGINE env var wins, otherwise the local default install.
if not defined UE_ENGINE set "UE_ENGINE=D:\UE_5.7"
set "ENGINE=%UE_ENGINE%"
if not exist "%ENGINE%\Engine\Binaries\Win64\UnrealEditor.exe" (
  echo UE 5.7 was not found at %ENGINE%.
  echo Set UE_ENGINE to your UE 5.7 install, e.g. setx UE_ENGINE "C:\Program Files\Epic Games\UE_5.7"
  pause
  exit /b 1
)
set "UE-LocalDataCachePath=%~dp0DerivedDataCache\FileCache"
start "Meteor Preview" "%ENGINE%\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0test.uproject" /Game/Voxel/Maps/Riverlands -ddc=InstalledNoZenLocalFallback -d3d11 -game -windowed -ResX=1440 -ResY=900 -NoSplash -MeteorPreview=0.65
