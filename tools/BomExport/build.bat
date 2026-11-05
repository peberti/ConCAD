@echo off
rem Builds BomExport.exe (File > Export BoM to Excel...) with the C# compiler
rem that ships with Windows (.NET Framework 4). No Visual Studio needed.
setlocal
cd /d "%~dp0"
set FW=%WINDIR%\Microsoft.NET\Framework\v4.0.30319
set CSC=%FW%\csc.exe
if not exist "%CSC%" (
	echo The .NET Framework 4 C# compiler was not found: %CSC%
	exit /b 1
)
"%CSC%" /nologo /codepage:65001 /target:exe /optimize+ /platform:anycpu /out:BomExport.exe /r:"%FW%\System.IO.Compression.dll" /r:"%FW%\System.IO.Compression.FileSystem.dll" /r:System.Xml.Linq.dll /r:System.Xml.dll /r:System.Core.dll BomExport.cs
if errorlevel 1 exit /b 1
echo Built BomExport.exe
