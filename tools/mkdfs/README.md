# Building with GCC
If building with GCC, please use the makefile in the root directory.

# Building an EXE for Windows using MSVC
If building for Windows, an extra file needs to be placed in the `include` directory (as it is not included as part of windows standard files)
A known working version can be found at: `https://raw.githubusercontent.com/tronkko/dirent/master/include/dirent.h`


To build the solution using From Powershell (with VS2017 installed), from the current directory, the following commands can be used:
```
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
cl .\tools\mkdfs\mkdfs.c -I .\include\
```
