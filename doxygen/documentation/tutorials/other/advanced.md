Manually configure your IDE or build tools {#advanced_build}
==========================
This document is useful if for some reason you chose not to use the CMake path, which is described here().

Suppose you need to embed Chrono features in a pre-existing
project of yours: maybe that none of the methods proposed [ in this
page](/documentation/tutorials/develop/) can fit your needs. For instance,
maybe you are using some other build tool (Jam, or Nmake, or in-house
tools), or you already have a pre-existing VC++ project and you are not
using CMake to manage it. In either cases, you can manually configure
your XXYYZZ build tool to use Chrono. In fact, regardless of the
build system, it all boils down to **including some headers** and
**linking a library**.

Here are some low-level guidelines in case you chose to manually
configure a build system.

![](Checkmark.png "fig:Checkmark.png")Compiler
----------------------------------------------

### Compiler includes

Projects based on Chrono must access the .h headers of the
library. Your C++ compiler must know this. The include directory is

`[path to Chrono::Engine]/source `

For example, if you installed in C:\\, the Microsoft C++ compiler will
take the following flag in the command line:

`/I "C:\ChronoEngine\source" `.

Another setting about includes is for the Irrlicht library, if you want
to use it for visualization. For Irrlicht, the include
directory is

`[path to Irrlicht]\include `

For example, if you installed Irrlicht in C:\\, the Microsoft C++
compiler will take the flag

`/I "C:\irrlicht-1.7.3\include" `

### Compiler flags and symbols

The compiler requires the following symbols to be defined:

**on window 32 bit**

-   WIN32
-   _CRT_SECURE_NO_DEPRECATE (optional)
-   _MINGW (only if MingW compiler)
-   _WINDOWS (only if MingW compiler)

**on window 64 bit**

-   WIN64
-   _CRT_SECURE_NO_DEPRECATE (optional)
-   _MINGW (only if MingW compiler)
-   _WINDOWS (only if MingW compiler)

**on Linux**

-   LINUX
-   __linux__

For example, with the Microsoft C++ compiler, you will add these flags
in the command line:

`/D "WIN32" /D "_CRT_SECURE_NO_DEPRECATE" `.

Also, note that in case of Microsoft compiler, you also must use the
flag

-   /MDd

(use debug dlls, multithreaded) when in debug mode.

Note: In order to prevent link-time issues and only if you are using the MingW GNU compiler, you may need to add
the following linker flags:

-   -Wl,--enable-runtime-pseudo-reloc

For example, using the Code::Blocks IDE, go to the menu Build/Compiler
Options, go to "Linker" tab, and insert the previous flags in the "Other
linker options" field.

![](Checkmark.png "fig:Checkmark.png")Linker
--------------------------------------------

The linker needs to know that it must link the Chrono library. To this end,
it requires its name and where to find that library.

The main ChronoEngine library is

-   ChronoEngine.lib

In case you want also 3D visualization using Irrlicht, you also need to
link

-   irrlicht.lib

Similarly, if you want to use additional units, ex. for MPI or
POSTPROCESSING or GPU features etc., you might need to link also
additional ChronoEngine libs, that is:

-   ChronoEngine_MPI.lib
-   ChronoEngine_POSTPROCESSING.lib
-   ...

Also, the compiler must know where to find these libraries (the
'/LIBPATH' flag in Microsoft linker, for instance). In Chrono,
the directory of the .lib depends on the type of the compiler you use, and if you need the release or debug versions.

For example, if you use Visual Studio for 32 bit, the libraries to link
are contained in the Chrono build directory, in the
lib\\Win32\\Debug or lib\\Win32\\Release directories, respectively (if
you compile your code in debug mode, you must link the library in the
Debug directory).

If you compile for 64 bit, you must link the libraries in
lib\\Win64\\Debug or lib\\Win64\\Release, etc. Note that libs of
different compilers cannot be mixed, even on the same platform. The same
for 32/64 bit versions, even from the same compiler: do not mix them!

Executables and DLLs
--------------------

After compilation, you usually get an .exe file of your project. That
executable must find the Chrono dll. There are different
methods to let the executable 'find' the DLLs. One is to copy and paste 
these DLL into the same directory with the executable. On Windows these files are:

-   ChronoEngine.dll
-   Irrlicht.dll (only if you are using Irrlicht for visualization).

Another option, is to add the \[path to Chrono::Engine dlls\] directory
to your PATH environment variable, or to register the dlls assemblies,
or similar methods.
