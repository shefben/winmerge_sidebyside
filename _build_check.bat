@echo off
set OUTFILE=F:\development\steam\emulator_bot\winmerge_sidebyside\_build_check.txt
echo Starting build check at %date% %time% > "%OUTFILE%"

"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "F:\development\steam\emulator_bot\winmerge_sidebyside\WinMerge.sln" /t:Merge /p:Configuration=Release /p:Platform=x64 /p:VcpkgApplocalDeps=false /p:VcpkgEnabled=false /p:VCPkgLocalAppDataDisabled=true /m /v:minimal >> "%OUTFILE%" 2>&1

echo. >> "%OUTFILE%"
echo CHECK_0 at %time%: >> "%OUTFILE%"
if exist "F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\WinMergeU.exe" (echo EXISTS >> "%OUTFILE%") else (echo MISSING >> "%OUTFILE%")

copy /Y "F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\WinMergeU.exe" "F:\development\steam\emulator_bot\winmerge_sidebyside\WinMergeU_saved.exe" >> "%OUTFILE%" 2>&1

ping -n 4 127.0.0.1 > nul
echo CHECK_3s at %time%: >> "%OUTFILE%"
if exist "F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\WinMergeU.exe" (echo EXISTS >> "%OUTFILE%") else (echo MISSING >> "%OUTFILE%")

ping -n 8 127.0.0.1 > nul
echo CHECK_10s at %time%: >> "%OUTFILE%"
if exist "F:\development\steam\emulator_bot\winmerge_sidebyside\Build\x64\Release\WinMergeU.exe" (echo EXISTS >> "%OUTFILE%") else (echo MISSING >> "%OUTFILE%")

echo Done at %time% >> "%OUTFILE%"
