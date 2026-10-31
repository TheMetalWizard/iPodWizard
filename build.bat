@echo off
rem Builds Release\iPodWizard.exe (Visual Studio 2005). Run build_firmware.bat first.
setlocal
rem Visual Studio 2005 sets VS80COMNTOOLS; the usual install folders are the fall back
set "VCVARS=%VS80COMNTOOLS%..\..\VC\vcvarsall.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio 8\VC\vcvarsall.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio 8\VC\vcvarsall.bat"
if not exist "%VCVARS%" goto novs
call "%VCVARS%" x86
cd /d "%~dp0"
if not exist Release mkdir Release
rc.exe /fo "Release\iPodWizard.res" "iPodWizard.rc"
if errorlevel 1 goto fail
cl.exe /c /nologo /W3 /O2 /Ob1 /Oi /Ot /GL /D WIN32 /D NDEBUG /D _WINDOWS /D _UNICODE /D UNICODE /D ZIP_ARCHIVE_MFC /EHsc /MT /GS /Gy /fp:precise /Zc:wchar_t /Zc:forScope /Fo"Release\\" /Fd"Release\vc80.pdb" /Gd /TP /I "." /I "Release" iPodWizard.cpp iPodWizardDlg.cpp LayoutDialog.cpp UpdaterDialog.cpp EditorDialog.cpp TweaksDialog.cpp FWExtractorDialog.cpp LayoutCommands.cpp LayoutLists.cpp LayoutInput.cpp
if errorlevel 1 goto fail
link.exe /OUT:"Release\iPodWizard.exe" /INCREMENTAL:NO /NOLOGO /MANIFEST /MANIFESTFILE:"Release\iPodWizard.exe.intermediate.manifest" /DEBUG /PDB:"Release\iPodWizard.pdb" /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /LTCG /MACHINE:X86 /ENTRY:wWinMainCRTStartup /NODEFAULTLIB:LIBC /NODEFAULTLIB:nafxcwd uafxcw.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wininet.lib comctl32.lib credui.lib version.lib "Zip.lib" Release\cgfiltyp.obj Release\Color.obj Release\DarkMode.obj Release\DialogMinHelpBtn.obj Release\EditorDialog.obj Release\firmware.obj Release\Font.obj Release\FontsDialog.obj Release\FontWnd.obj Release\HelpDialog.obj Release\hexeditor.obj Release\HScrollListBox.obj Release\HyperLink.obj Release\iPodWizard.obj Release\iPodWizardDlg.obj Release\LayoutDialog.obj Release\LayoutNames.obj Release\PixoScript.obj Release\CommandTable.obj Release\LayoutCommands.obj Release\LayoutAttrs.obj Release\LayoutLists.obj Release\LayoutInput.obj Release\MFCSink.obj Release\OTFDialog.obj Release\OTFFontParser.obj Release\OTFFontWnd.obj Release\Picture.obj Release\PicturesDialog.obj Release\PictureWnd.obj Release\PrefsDialog.obj Release\RDragDropFiles.obj Release\ResourceManager.obj Release\ScanDialog.obj Release\stdafx.obj Release\StringDialog.obj Release\ThemePreviewer.obj Release\ThemesDialog.obj Release\ThemeWnd.obj Release\TipDlg.obj Release\ToolTipListCtrl.obj Release\TweaksDialog.obj Release\UpdaterDialog.obj Release\FWExtractorDialog.obj Release\VisualStylesXP.obj Release\iPodWizard.res
if errorlevel 1 goto fail
copy /Y PixoNames.txt Release\ >nul
copy /Y iPW.ini Release\ >nul
echo.
echo BUILD OK: Release\iPodWizard.exe
goto :eof
:fail
echo.
echo BUILD FAILED
exit /b 1
:novs
echo Microsoft Visual Studio 2005 was not found. Run this script from a Visual Studio 2005 Command Prompt.
exit /b 1
