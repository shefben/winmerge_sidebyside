@echo off
echo Starting rebuild at %date% %time% ...
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "F:\development\steam\emulator_bot\winmerge_sidebyside\WinMerge.sln" /t:Merge /p:Configuration=Release /p:Platform=x64 /p:VcpkgApplocalDeps=false /p:VcpkgEnabled=false /p:VCPkgLocalAppDataDisabled=true /m /verbosity:minimal
set BUILDRC=%errorlevel%
echo Build exit code: %BUILDRC%
if exist "F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\WinMergeU.exe" (
    echo EXE EXISTS
    for %%F in ("F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\WinMergeU.exe") do echo EXE size: %%~zF timestamp: %%~tF
) else (
    echo EXE MISSING - checking directory...
    dir "F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\" 2>nul
)
echo Done at %date% %time%
