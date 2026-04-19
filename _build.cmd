@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
MSBuild WinMerge.sln /t:Merge /p:Configuration=Release /p:Platform=x64 /p:VcpkgApplocalDeps=false /m /v:minimal
echo BUILDEXITCODE=%ERRORLEVEL%
