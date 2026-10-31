# iPodWizard
Originally created in 2004, iPW is an iPod firmware modification utility which allows you to customize the Stock OS of Clickwheel based iPods. From themes which modify the graphics to low level modifications of the firmware itself, it has long been an essential tool in the iPod modding scene.

## How to run iPodWizard
1. Download the program from the releases tab
2. Extract all files
3. PixoNames.txt & iPW.ini MUST be in the same folder as iPodWizard.exe
4. (Optional) Run the program as administrator (This is required to gain access to connected iPods)
5. Begin editing!

## Building from source
#### Requirements:
- Microsoft Visual Studio 2005 with MFC and Visual C++
- Static Unicode MFC Library (This is not included in VS2005 express)
- The entire source, including the ZipArchive project.

#### VS2005 IDE building:
1. Open './iPodWizard.sln'
2. Select 'Release' configuration (Win32)
3. Build the iPodWizard project
   - You can also build the whole Solution, although this is not needed since './Zip.lib' is prebuilt
4. The program will build into './Release/iPodWizard.exe'
5. Copy 'PixoNames.txt', 'iPW.ini', 'iPodWizard.exe' to a new folder to use.

#### (Optional) Rebuilding Zip.lib
1. Open './zip/ZipArchive.vcproj' or './iPodWizard.sln'
2. Select 'Unicode MFC Static' configuration (Win32)
3. Build the ZipArchive project
4. Copy the new 'Zip.lib' file from './zip/Release Unicode MFC Static' to the root folder

#### CLI building:
1. Open source folder in Command Prompt
2. Run 'build_firmware.bat'
   Builds the support source files into './Release'
3. Run 'build.bat'
   Builds the resources, copies 'PixoNames.txt', 'iPW.ini', and links ./Release/iPodWizard.exe
4. Run ./Release/iPodWizard.exe

If you modify the source, run both scripts in order to link the modified files together.
You may need to delete the ./Release folder if linking fails.

#### Common problems:
1. If building from the CLI, "Microsoft Visual Studio 2005 was not found"
- Run the scripts from a VS2005 Command Prompt
- Your VS2005 install may be in the wrong directory
2. When building, "LNK1104: cannot open file "Release\iPodWizard.exe"
- './Release/iPodWizard.exe' is currently running. Close it, or if it's frozen, close from task manager.
3. Various missing .obj or unresolved external symbol errors
- Scripts ran in the wrong order. Ensure './build_firmware.bat' is ran before './build.bat'

#### Optimisation & Configuration:
Release: /O2 /Ob2 /Oi /Ot /Oy /GF
Whole program: /GL with /LTCG at the link
Static MFC: /MT
Unicode, x86 (32-bit)

The scripts build at the highest speed settings above. Reducing the speed will reduce the size of the program. There is no detectable change in performance of the program.

Releases on this page were built at the highest optimisation settings.

The ZipArchive library in './zip' is copyright
2000 - 2006 by Tadeusz Dracz under GNU GPL V2.0
License can be found in the ZipArchive folder.
