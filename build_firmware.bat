@echo off
rem Compiles the support files of iPodWizard into Release\ (Visual Studio 2005). Run it before build.bat.
setlocal
rem Visual Studio 2005 sets VS80COMNTOOLS; the usual install folders are the fall back
set "VCVARS=%VS80COMNTOOLS%..\..\VC\vcvarsall.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio 8\VC\vcvarsall.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio 8\VC\vcvarsall.bat"
if not exist "%VCVARS%" goto novs
call "%VCVARS%" x86
cd /d "%~dp0"
if not exist Release mkdir Release
cl.exe /c /nologo /W3 /O2 /Ob2 /Oi /Ot /Oy /GF /GL /D WIN32 /D NDEBUG /D _WINDOWS /D _UNICODE /D UNICODE /D ZIP_ARCHIVE_MFC /EHsc /MT /GS /Gy /fp:precise /Zc:wchar_t /Zc:forScope /Fo"Release\\" /Fd"Release\vc80.pdb" /Gd /TP /I "." /I "Release" firmware.cpp Font.cpp Picture.cpp OTFFontParser.cpp stdafx.cpp Color.cpp DarkMode.cpp cgfiltyp.cpp DialogMinHelpBtn.cpp FontWnd.cpp HelpDialog.cpp hexeditor.cpp HScrollListBox.cpp HyperLink.cpp MFCSink.cpp OTFFontWnd.cpp PictureWnd.cpp PrefsDialog.cpp RDragDropFiles.cpp ResourceManager.cpp ScanDialog.cpp StringDialog.cpp ThemePreviewer.cpp ThemeWnd.cpp ThemesDialog.cpp TipDlg.cpp ToolTipListCtrl.cpp FontsDialog.cpp OTFDialog.cpp PicturesDialog.cpp LayoutNames.cpp PixoScript.cpp CommandTable.cpp LayoutAttrs.cpp VisualStylesXP.cpp
if errorlevel 1 (echo COMPILE FAILED) else (echo COMPILE OK)
goto :eof
:novs
echo Microsoft Visual Studio 2005 was not found. Run this script from a Visual Studio 2005 Command Prompt.
exit /b 1
