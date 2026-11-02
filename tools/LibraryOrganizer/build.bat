@echo off
rem Builds LibraryOrganizer.exe with the C# compiler that ships with Windows
rem (.NET Framework 4). No Visual Studio needed.
setlocal
cd /d "%~dp0"
set CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe
if not exist "%CSC%" (
	echo The .NET Framework 4 C# compiler was not found: %CSC%
	exit /b 1
)
"%CSC%" /nologo /codepage:65001 /target:winexe /optimize+ /platform:anycpu /win32icon:..\..\src\res\idr_main.ico /out:LibraryOrganizer.exe /r:System.Windows.Forms.dll /r:System.Drawing.dll LibraryOrganizer.cs
if errorlevel 1 exit /b 1
echo Built %~dp0LibraryOrganizer.exe
