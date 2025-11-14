# Html-TADS
Unlike [tads-runner](https://github.com/captain-mayhem/tads-runner), which is licensed under GPL-2.0, the htmltads subfolder comes with a very restrictive license.

From the license file of htmltads:
"This source code is distributed for the specific purpose of
facilitating the creation of versions of TADS on various computers and
operating systems. All other derivative works are prohibited without
the written permission of the author.
...
If you port TADS to a new platform, the author does grant permission
for you to distribute your ported version - I encourage it, in fact."

Since my goal is only to recompile htmltads as a 64-bit executable running on a Windows 11 PC 
with a modern compiler and a CMake buildsystem without changing the feature set of htmltads,
this project is supposed to fall under the porting TADS exception.

## How to build

### Prerequisites to install
- Visual Studio 2022 with C++ Compiler
- CMake (Version 3.x recommended, 4.x untested so far)
- git

### Directory setup
- git clone https://github.com/captain-mayhem/tads-runner.git
- git clone https://github.com/captain-mayhem/htmltads.git
- create an empty folder parallel to the two git repositories as build directory, e.g. build_tads

### Build instructions
- Open a Visual Studio "x64 Native Tools Command Prompt for VS 2022"
- enter your build directory (cd build_tads)
- Call CMake: cmake -G "Visual Studio 17 2022" -DCMAKE_INSTALL_PREFIX=install ..\tads-runner
- This will create a Tads.sln solution file in your build Directory
- Double click on it to open Visual Studio
- In the drop down, switch from "Debug" to "Release" (unless you want to build a Debug version)
- Build solution
- Build the target INSTALL to create the folder named install in the build directory that contains the build result
- To create a zip file, you can build the PACKAGE target
