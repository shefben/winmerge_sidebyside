@echo off
echo Starting build at %date% %time% ...
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "F:\development\steam\emulator_bot\winmerge_sidebyside\WinMerge.sln" /t:Merge /p:Configuration=Release /p:Platform=x64 /p:VcpkgApplocalDeps=false /p:VcpkgEnabled=false /p:VCPkgLocalAppDataDisabled=true /m > "F:\development\steam\emulator_bot\winmerge_sidebyside\_build_out.txt" 2> "F:\development\steam\emulator_bot\winmerge_sidebyside\_build_err.txt"
set BUILDRC=%errorlevel%
echo Build exit code: %BUILDRC% >> "F:\development\steam\emulator_bot\winmerge_sidebyside\_build_out.txt"
echo Build finished at %date% %time% >> "F:\development\steam\emulator_bot\winmerge_sidebyside\_build_out.txt"
if exist "F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\WinMergeU.exe" (
    echo EXE EXISTS >> "F:\development\steam\emulator_bot\winmerge_sidebyside\_build_out.txt"
    for %%F in ("F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\WinMergeU.exe") do echo EXE timestamp: %%~tF size: %%~zF >> "F:\development\steam\emulator_bot\winmerge_sidebyside\_build_out.txt"
    copy /Y "F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\WinMergeU.exe" "F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\WinMergeU_backup.exe" >> "F:\development\steam\emulator_bot\winmerge_sidebyside\_build_out.txt"
) else (
    echo EXE MISSING >> "F:\development\steam\emulator_bot\winmerge_sidebyside\_build_out.txt"
)
